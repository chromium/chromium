// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ANDROID_AUTOFILL_BROWSER_AUTOFILL_PROVIDER_ANDROID_H_
#define COMPONENTS_ANDROID_AUTOFILL_BROWSER_AUTOFILL_PROVIDER_ANDROID_H_

#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "components/android_autofill/browser/autofill_provider.h"
#include "components/android_autofill/browser/autofill_provider_android_bridge.h"
#include "components/android_autofill/browser/form_data_android.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/common/unique_ids.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class WebContents;
}

namespace autofill {

class TouchToFillKeyboardSuppressor;

// Android implementation of AutofillProvider, it has one instance per
// WebContents, this class is native peer of AutofillProvider.java.
// This class is always instantialized by AutofillProvider Java object.
class AutofillProviderAndroid : public AutofillProvider,
                                public AutofillProviderAndroidBridge::Delegate,
                                public content::WebContentsObserver {
 public:
  // Used as a metric that describes the state of a prefill requests. It is
  // emitted for every form deemed suitable for prefill requests prefill
  // requests are supported on this Android version (U+).
  enum class PrefillRequestState {
    // A prefill request was sent, a view structure was requested and provided
    // and a bottom sheet was shown.
    kRequestSentStructureProvidedBottomSheetShown = 0,
    // A prefill request was sent and a view structure was requested and
    // provided, but no bottom sheet was shown. The reason for not showing the
    // bottom sheet is opaque to WebView (e.g., no suggestions available by
    // providers, keyboard suppression not working correctly, etc.)
    kRequestSentStructureProvidedBottomSheetNotShown = 1,
    // A prefill request was sent, but no view structure was requested by the
    // framework.
    kRequestSentStructureNotProvided = 2,
    // A prefill request was sent, but the form changed substantially between
    // sending the cache request and the focus event on the form.
    kRequestSentFormChanged = 3,
    // A prefill request was not sent because the maximum number of prefill
    // requests (currently 1 per session) had already been reached.
    kRequestNotSentMaxNumberReached = 4,
    // A prefill request was not sent because there was no time - the form was
    // only analyzed to be cacheable when a session had already been started.
    kRequestNotSentNoTime = 5,
    kMaxValue = kRequestNotSentNoTime
  };

  static constexpr char kPrefillRequestStateUma[] =
      "Autofill.WebView.PrefillRequestState";

  static void CreateForWebContents(content::WebContents* web_contents);

  static AutofillProviderAndroid* FromWebContents(
      content::WebContents* web_contents);

  AutofillProviderAndroid(const AutofillProviderAndroid&) = delete;
  AutofillProviderAndroid& operator=(const AutofillProviderAndroid&) = delete;
  ~AutofillProviderAndroid() override;

  // Attach this detached object to `jcaller`.
  void AttachToJavaAutofillProvider(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& jcaller);

  // AutofillProvider:
  void OnAskForValuesToFill(
      AndroidAutofillManager* manager,
      const FormData& form,
      const FormFieldData& field,
      const gfx::RectF& bounding_box,
      AutofillSuggestionTriggerSource /*unused_trigger_source*/) override;
  void OnTextFieldDidChange(AndroidAutofillManager* manager,
                            const FormData& form,
                            const FormFieldData& field,
                            const gfx::RectF& bounding_box,
                            const base::TimeTicks timestamp) override;
  void OnTextFieldDidScroll(AndroidAutofillManager* manager,
                            const FormData& form,
                            const FormFieldData& field,
                            const gfx::RectF& bounding_box) override;
  void OnSelectControlDidChange(AndroidAutofillManager* manager,
                                const FormData& form,
                                const FormFieldData& field,
                                const gfx::RectF& bounding_box) override;
  void OnFormSubmitted(AndroidAutofillManager* manager,
                       const FormData& form,
                       bool known_success,
                       mojom::SubmissionSource source) override;
  void OnFocusNoLongerOnForm(AndroidAutofillManager* manager,
                             bool had_interacted_form) override;
  void OnFocusOnFormField(AndroidAutofillManager* manager,
                          const FormData& form,
                          const FormFieldData& field,
                          const gfx::RectF& bounding_box) override;
  void OnDidFillAutofillFormData(AndroidAutofillManager* manager,
                                 const FormData& form,
                                 base::TimeTicks timestamp) override;
  void OnHidePopup(AndroidAutofillManager* manager) override;
  void OnServerPredictionsAvailable(AndroidAutofillManager& manager,
                                    FormGlobalId form_id) override;
  void OnServerQueryRequestError(AndroidAutofillManager* manager,
                                 FormSignature form_signature) override;

  void OnManagerResetOrDestroyed(AndroidAutofillManager* manager) override;

  bool GetCachedIsAutofilled(const FormFieldData& field) const override;

  void MaybeInitKeyboardSuppressor() override;

 private:
  friend class AutofillProviderAndroidTestApi;

  explicit AutofillProviderAndroid(content::WebContents* web_contents);

  // AndroidAutofillProviderBridge::Delegate:
  void OnAutofillAvailable() override;
  void OnAcceptDatalistSuggestion(const std::u16string& value) override;
  void SetAnchorViewRect(const base::android::JavaRef<jobject>& anchor,
                         const gfx::RectF& bounds) override;
  void OnShowBottomSheetResult(bool is_shown,
                               bool provided_autofill_structure) override;

  // content::WebContentsObserver:
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void OnVisibilityChanged(content::Visibility visibility) override;

  // Returns true the first time it is called after a bottom sheet was shown.
  // This is intended to be used only by the keyboard suppressor, which calls it
  // once in OnAfterAskForValuesToFill to determine whether it should continue
  // to suppress the keyboard. Ideally, this would return whether the bottom
  // sheet is currently showing, but Android does not expose that information.
  bool WasBottomSheetJustShown(AutofillManager& manager);

  // Returns true if there's possibility for the bottom sheet to show, false
  // otherwise.
  bool IntendsToShowBottomSheet(AutofillManager& manager,
                                FormGlobalId form,
                                FieldGlobalId field,
                                const FormData& form_data) const;

  void FireSuccessfulSubmission(mojom::SubmissionSource source);

  // Calls `OnFormFieldDidChange` in the bridge if there is an ongoing Autofill
  // session for this `form`.
  void MaybeFireFormFieldDidChange(AndroidAutofillManager* manager,
                                   const FormData& form,
                                   const FormFieldData& field,
                                   const gfx::RectF& bounding_box);

  // Propagates visibility changes for fields in `form` and notifies the bridge
  // in case any of the fields had a visibility change.
  void MaybeFireFormFieldVisibilitiesDidChange(AndroidAutofillManager* manager,
                                             const FormData& form);

  bool IsLinkedManager(AndroidAutofillManager* manager) const;

  // Checks whether a `form_` is linked and whether it's the same form as
  // `form`, having same identifier.
  bool IsIdOfLinkedForm(FormGlobalId form_id) const;

  // Same as `IsLinkedForm`, but also checks that `form` and `form_` are
  // similar, using form similarity checks.
  bool IsLinkedForm(const FormData& form) const;

  gfx::RectF ToClientAreaBound(const gfx::RectF& bounding_box);

  void StartNewSession(AndroidAutofillManager* manager,
                       const FormData& form,
                       const FormFieldData& field,
                       const gfx::RectF& bounding_box);

  void Reset();

  // Returns a new session id. Session ids are required when creating a
  // `FormDataAndroid` object and used to generate virtual ids that identify
  // form fields uniquely to the Android Autofill framework.
  SessionId CreateSessionId();

  // Returns whether prefill requests are supported. This depends on the
  // Android version.
  bool ArePrefillRequestsSupported() const;

  // Sends a prefill request to the Autofill framework if all the below
  // conditions are met:
  // 1. Prefill requests are supported (correct SDK version & feature flag).
  // 2. No prefill request has been sent so far, since the framework only
  // supports caching a single form at a time.
  // 3. There is no ongoing Autofill session. This is to ensure that the
  // `onProvideAutofillStructure` callback from the framework does not confuse
  // information requests for caching and for the current Autofill session.
  // 4. The form is predicted to be a login form.
  void MaybeSendPrefillRequest(const AndroidAutofillManager& manager,
                               FormGlobalId form_id);

  // This is used by the keyboard suppressor. We update it with the result of
  // the platform method call `showAutofillDialog`. Since we are not notified
  // when the bottom sheet is dismissed, we set a timer to set it to `false`
  // shortly after `WasBottomSheetJustShown()` is called. The timer's function
  // is to handle multiple calls related to the same user event correctly, which
  // can currently happen (crbug.com/1490581).
  bool was_bottom_sheet_just_shown_ = false;

  // In some cases we get two AskForValuesToFill events within short time frame
  // so we set timer to set the `was_bottom_sheet_just_shown_` to false after it
  // gets accessed.
  // TODO(crbug.com/1490581): Remove once a fix is landed on the renderer side.
  void SetBottomSheetShownOff();

  // Sets `was_bottom_sheet_just_shown` to false after a timeout.
  base::OneShotTimer was_shown_bottom_sheet_timer_;

  // The form for which a prefill request has been sent.
  std::unique_ptr<FormDataAndroid> cached_form_;

  // Indicates whether we have used the cached form to show a bottom sheet. This
  // state is kept because a bottom sheet should only be shown once per cached
  // form to allow the user to access the keyboard after focusing on the
  // (cached) form a second time.
  bool has_used_cached_form_ = false;

  // The form of the current session (queried input or changed select box).
  std::unique_ptr<FormDataAndroid> form_;

  // Properties of the field of the current session (queried input or changed
  // select box).
  FieldGlobalId field_id_;
  FieldTypeGroup field_type_group_{FieldTypeGroup::kNoGroup};

  // The frame of the field for which the last OnAskForValuesToFill() happened.
  //
  // It is not necessarily the same frame as the current session's
  // `field_id_.host_frame` because the session may survive
  // OnAskForValuesToFill().
  //
  // It's not necessarily the same frame as `manager_`'s for the same reason as
  // `field_id_`, and also because `manager_` may refer to an ancestor frame of
  // the queried field.
  content::GlobalRenderFrameHostId last_queried_field_rfh_id_;

  // The origin of the field of the current session (cf. `field_id_`). This is
  // determines which fields are safe to be filled in cross-frame forms.
  url::Origin triggered_origin_;
  base::WeakPtr<AndroidAutofillManager> manager_;
  bool check_submission_ = false;
  // Valid only if check_submission_ is true.
  mojom::SubmissionSource pending_submission_source_;

  static constexpr SessionId kMinimumSessionId = SessionId(1);
  static constexpr SessionId kMaximumSessionId = SessionId(0xffff);
  // The last assigned session id.
  SessionId last_session_id_ = kMaximumSessionId;

  // The bridge for C++ <-> Java communication.
  std::unique_ptr<AutofillProviderAndroidBridge> bridge_;

  // Used for handling keyboard suppression in case there's a bottom sheet.
  std::unique_ptr<TouchToFillKeyboardSuppressor> keyboard_suppressor_;
};
}  // namespace autofill

#endif  // COMPONENTS_ANDROID_AUTOFILL_BROWSER_AUTOFILL_PROVIDER_ANDROID_H_
