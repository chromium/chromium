// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ANDROID_AUTOFILL_BROWSER_ANDROID_AUTOFILL_PROVIDER_H_
#define COMPONENTS_ANDROID_AUTOFILL_BROWSER_ANDROID_AUTOFILL_PROVIDER_H_

#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "components/android_autofill/browser/android_autofill_provider_bridge.h"
#include "components/android_autofill/browser/autofill_provider.h"
#include "components/android_autofill/browser/form_data_android.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/webauthn/android/webauthn_cred_man_delegate.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class WebContents;
}  // namespace content

namespace password_manager {
struct PasswordForm;
}  // namespace password_manager

namespace autofill {

class TouchToFillKeyboardSuppressor;

// Android implementation of AutofillProvider, it has one instance per
// WebContents, this class is native peer of AutofillProvider.java.
// This class is always instantialized by AutofillProvider Java object.
class AndroidAutofillProvider : public AutofillProvider,
                                public AndroidAutofillProviderBridge::Delegate,
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
  // The name of the UMA that is emitted when a form similarity check between a
  // cached form and the interacted form fails.
  static constexpr char kPrefillRequestBottomsheetNoViewStructureDelayUma[] =
      "Autofill.WebView.BottomsheetNoViewStructureDelay";

  static void CreateForWebContents(content::WebContents* web_contents);

  static AndroidAutofillProvider* FromWebContents(
      content::WebContents* web_contents);

  AndroidAutofillProvider(const AndroidAutofillProvider&) = delete;
  AndroidAutofillProvider& operator=(const AndroidAutofillProvider&) = delete;
  ~AndroidAutofillProvider() override;

  // Attach this detached object to `jcaller`.
  void AttachToJavaAutofillProvider(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& jcaller);

  // AutofillProvider:
  void OnAskForValuesToFill(
      AndroidAutofillManager* manager,
      const FormData& form,
      const FormFieldData& field,
      AutofillSuggestionTriggerSource /*unused_trigger_source*/) override;
  void OnTextFieldDidChange(AndroidAutofillManager* manager,
                            const FormData& form,
                            const FormFieldData& field,
                            const base::TimeTicks timestamp) override;
  void OnTextFieldDidScroll(AndroidAutofillManager* manager,
                            const FormData& form,
                            const FormFieldData& field) override;
  void OnSelectControlDidChange(AndroidAutofillManager* manager,
                                const FormData& form,
                                const FormFieldData& field) override;
  void OnFormSubmitted(AndroidAutofillManager* manager,
                       const FormData& form,
                       bool known_success,
                       mojom::SubmissionSource source) override;
  void OnFocusOnNonFormField(AndroidAutofillManager* manager) override;
  void OnFocusOnFormField(AndroidAutofillManager* manager,
                          const FormData& form,
                          const FormFieldData& field) override;
  void OnDidFillAutofillFormData(AndroidAutofillManager* manager,
                                 const FormData& form,
                                 base::TimeTicks timestamp) override;
  void OnHidePopup(AndroidAutofillManager* manager) override;
  void OnServerPredictionsAvailable(AndroidAutofillManager& manager,
                                    FormGlobalId form_id) override;

  void OnManagerResetOrDestroyed(AndroidAutofillManager* manager) override;

  bool GetCachedIsAutofilled(const FormFieldData& field) const override;

  void MaybeInitKeyboardSuppressor() override;

 private:
  friend class AndroidAutofillProviderTestApi;

  explicit AndroidAutofillProvider(content::WebContents* web_contents);

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

  // Updates fields that changed on native class only. The Android bridge is not
  // yet invoked to give preference to a possible CredMan flow.
  // Using the given form and field that are newly focused, the method returns
  // the necessary field information to continue the focus event later on. If
  // continuing the focus event is not possible or necessary, it returns a
  // `std::nullopt`.
  std::optional<AndroidAutofillProviderBridge::FieldInfo> StartFocusChange(
      const FormData& form,
      const FormFieldData& field);

  // Calls `OnFormFieldDidChange` in the bridge if there is an ongoing Autofill
  // session for this `form`.
  void MaybeFireFormFieldDidChange(AndroidAutofillManager* manager,
                                   const FormData& form,
                                   const FormFieldData& field);

  bool IsLinkedManager(AndroidAutofillManager* manager) const;

  // Checks whether a `form_` is linked and whether it's the same form as
  // `form`, having same identifier.
  bool IsIdOfLinkedForm(FormGlobalId form_id) const;

  // Same as `IsLinkedForm`, but also checks that `form` and `form_` are
  // similar, using form similarity checks.
  bool IsLinkedForm(const FormData& form) const;

  gfx::RectF ToClientAreaBound(const gfx::RectF& bounding_box) const;

  void StartNewSession(AndroidAutofillManager* manager,
                       const FormData& form,
                       const FormFieldData& field);

  void Reset();

  // Cancels the current Autofill session, resetting cached session data.
  void CancelSession();

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
  //    supports caching a single form at a time.
  // 3. There is no ongoing Autofill session. This is to ensure that the
  //    `onProvideAutofillStructure` callback from the framework does not
  //     confuse information requests for caching and for the current Autofill
  //     session.
  // 4. The form is predicted to be a login form or a (assuming that
  //    `kAndroidAutofillPrefillRequestsForChangePassword` is enabled) a change
  //     password form.
  void MaybeSendPrefillRequest(const AndroidAutofillManager& manager,
                               FormGlobalId form_id);

  // Stores field ids for fields detected by `password_manager::FormDataParser`
  // as username or password fields. Currently used only for prefill requests.
  struct PasswordParserOverrides {
    bool operator==(const PasswordParserOverrides&) const = default;

    // Returns the `PasswordParserOverrides` obtained from matching the
    // `FieldRendererId`s of username and password fields in `pw_form` to the
    // `FieldGlobalId`s in `form_structure`. Returns `std::nullopt` if no unique
    // matching could be found or if the matching is incomplete. A unique
    // matching may not exist if the form is spread across multiple iframes. In
    // practice, this should be extremely rare for password forms.
    static std::optional<PasswordParserOverrides> FromPasswordForm(
        const password_manager::PasswordForm& pw_form,
        const FormStructure& form_structure);

    // Creates a map as expected by `FormDataAndroid::UpdateFieldTypes`.
    base::flat_map<FieldGlobalId, AutofillType> ToFieldTypeMap() const;

    std::optional<FieldGlobalId> username_field_id;
    std::optional<FieldGlobalId> password_field_id;
    std::optional<FieldGlobalId> new_password_field_id;
  };

  // Checks whether `form` is similar to the cached form. `form_structure` must
  // be the `form_structure` corresponding to `form` if it is available (i.e.
  // cached by the AutofillManager already). The check works as follows:
  // - If `form_structure` is not null and
  //   `kAndroidAutofillSignatureForPrefillRequestSimilarityCheck` is enabled,
  //   the form is parsed using `password_manager::FormDataParser`. The form
  //   is classified as similar if the fields classified as username and
  //   passwords match between the cached and the focused form.
  // - Alternatively, it returns the result of `FormDataAndroid::SimilarFormAs`.
  bool IsFormSimilarToCachedForm(const FormData& form,
                                 const FormStructure* form_structure) const;

  // In some cases we get two AskForValuesToFill events within short time frame
  // so we set timer to set the `was_bottom_sheet_just_shown_` to false after it
  // gets accessed.
  // TODO(crbug.com/40284788): Remove once a fix is landed on the renderer side.
  void SetBottomSheetShownOff();

  // Stops the keyboard suppression. Called when the CredMan UI was closed. If
  // the UI was dismissed without selecting a passkey, `success` will be false.
  // If a `field_to_focus` is given and CredMan wasn't able to sign the
  // user in, attempt to continue an earlier attempt to focus a field.
  void OnCredManUiClosed(
      FormGlobalId form_id,
      std::optional<AndroidAutofillProviderBridge::FieldInfo> field_to_focus,
      bool success);

  // Returns true if CredMan *may* be shown for the given field. It only returns
  // false if the sheet was already shown or prefetching concluded and indicated
  // that no passkeys are available.
  bool IntendsToShowCredMan(const FormFieldData& field,
                            content::RenderFrameHost* rfh) const;

  // Returns true if a passkey request is pending  or succeeded for the given
  // `rfh` and the CredMan UI should be shown when the given `field` is focused.
  bool ShouldShowCredManForField(const FormFieldData& field,
                                 content::RenderFrameHost* rfh);

  // Triggers a prefetched passkey request which opens a bottom sheet. The given
  // `field_to_focus` is used to continue any interrupted focus event once
  // CredMan closes. Returns true if the sheet was opened and false if that
  // wasn't possible.
  bool ShowCredManSheet(
      content::RenderFrameHost* rfh,
      FormGlobalId form_id,
      std::optional<AndroidAutofillProviderBridge::FieldInfo> field_to_focus);

  enum class CredManBottomSheetLifecycle {
    kNotShown,   // The sheet hasn't been shown. Does not indicate it will be.
    kIsShowing,  // The sheet was triggered. Does not guarantee it's visible.
    kClosed,     // The sheet was dismissed and shouldn't be shown again.
  };

  CredManBottomSheetLifecycle credman_sheet_status_ =
      CredManBottomSheetLifecycle::kNotShown;

  // This is used by the keyboard suppressor. We update it with the result of
  // the platform method call `showAutofillDialog`. Since we are not notified
  // when the bottom sheet is dismissed, we set a timer to set it to `false`
  // shortly after `WasBottomSheetJustShown()` is called. The timer's function
  // is to handle multiple calls related to the same user event correctly, which
  // can currently happen (crbug.com/1490581).
  bool was_bottom_sheet_just_shown_ = false;

  // Sets `was_bottom_sheet_just_shown` to false after a timeout.
  base::OneShotTimer was_shown_bottom_sheet_timer_;

  // Helper struct that contains cache data used in prefill requests.
  struct CachedData {
    CachedData();
    CachedData(CachedData&&);
    CachedData& operator=(CachedData&&);
    ~CachedData();

    std::unique_ptr<FormDataAndroid> cached_form;
    PasswordParserOverrides password_parser_overrides;
    // The time when the prefill request was sent - used for metrics only.
    base::TimeTicks prefill_request_creation_time;
  };
  std::optional<CachedData> cached_data_;

  // Indicates whether we have used the cached form to show a bottom sheet. This
  // state is kept because a bottom sheet should only be shown once per cached
  // form to allow the user to access the keyboard after focusing on the
  // (cached) form a second time.
  bool has_used_cached_form_ = false;

  // The form of the current session (queried input or changed select box).
  std::unique_ptr<FormDataAndroid> form_;

  // Properties of the last-focused field of the current session for `form_`
  // (queried input or changed select box).
  struct {
    FieldGlobalId id;
    FieldTypeGroup group = {FieldTypeGroup::kNoGroup};
    url::Origin origin;
  } current_field_;

  // The frame of the field for which the last OnAskForValuesToFill() happened.
  //
  // It is not necessarily the same frame as the current session's
  // `last_focused_field_id_.host_frame` because the session may survive
  // OnAskForValuesToFill().
  //
  // It's not necessarily the same frame as `manager_`'s for the same reason as
  // `last_focused_field_id_`, and also because `manager_` may refer to an
  // ancestor frame of the queried field.
  content::GlobalRenderFrameHostId last_queried_field_rfh_id_;

  base::WeakPtr<AndroidAutofillManager> manager_;
  bool check_submission_ = false;
  // Valid only if check_submission_ is true.
  mojom::SubmissionSource pending_submission_source_;

  static constexpr SessionId kMinimumSessionId = SessionId(1);
  static constexpr SessionId kMaximumSessionId = SessionId(0xffff);
  // The last assigned session id.
  SessionId last_session_id_ = kMaximumSessionId;

  // The bridge for C++ <-> Java communication.
  std::unique_ptr<AndroidAutofillProviderBridge> bridge_;

  // Used for handling keyboard suppression in case there's a bottom sheet.
  std::unique_ptr<TouchToFillKeyboardSuppressor> keyboard_suppressor_;

  base::WeakPtrFactory<AndroidAutofillProvider> weak_ptr_factory_{this};
};
}  // namespace autofill

#endif  // COMPONENTS_ANDROID_AUTOFILL_BROWSER_ANDROID_AUTOFILL_PROVIDER_H_
