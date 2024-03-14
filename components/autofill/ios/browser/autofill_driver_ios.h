// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_DRIVER_IOS_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_DRIVER_IOS_H_

#import <string>

#import "base/containers/flat_map.h"
#import "base/containers/flat_set.h"
#import "base/containers/span.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/raw_ref.h"
#import "base/memory/weak_ptr.h"
#import "components/autofill/core/browser/autofill_client.h"
#import "components/autofill/core/browser/browser_autofill_manager.h"
#import "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#import "ios/web/public/js_messaging/web_frame_user_data.h"
#import "url/origin.h"

namespace web {
class WebFrame;
class WebState;
}

@protocol AutofillDriverIOSBridge;

namespace autofill {

class AutofillDriverIOSFactory;

// AutofillDriverIOS drives the Autofill flow in the browser process based
// on communication from JavaScript and from the external world.
//
// AutofillDriverIOS communicates with an AutofillDriverIOSBridge, which in
// Chrome is implemented by AutofillAgent, and a BrowserAutofillManager.
//
// AutofillDriverIOS is associated with exactly one WebFrame and its lifecycle
// is bound to that WebFrame.
class AutofillDriverIOS : public AutofillDriver,
                          public AutofillManager::Observer,
                          public web::WebFrameUserData<AutofillDriverIOS> {
 public:
  // Returns the AutofillDriverIOS for `web_state` and `web_frame`. Creates the
  // driver if necessary.
  static AutofillDriverIOS* FromWebStateAndWebFrame(web::WebState* web_state,
                                                    web::WebFrame* web_frame);

  // Convenience method that grabs the frame associated with `token` and returns
  // the associated driver. Creates the driver if `token` refers to a valid
  // frame but no driver exists; returns nullptr if `token` does not refer to a
  // valid frame.
  static AutofillDriverIOS* FromWebStateAndLocalFrameToken(
      web::WebState* web_state,
      LocalFrameToken token);

  ~AutofillDriverIOS() override;

  // AutofillDriver:
  LocalFrameToken GetFrameToken() const override;
  std::optional<LocalFrameToken> Resolve(FrameToken query) override;
  AutofillDriverIOS* GetParent() override;
  AutofillClient& GetAutofillClient() override;
  BrowserAutofillManager& GetAutofillManager() override;
  bool IsInActiveFrame() const override;
  bool IsInAnyMainFrame() const override;
  bool IsPrerendering() const override;
  bool HasSharedAutofillPermission() const override;
  bool CanShowAutofillUi() const override;
  base::flat_set<FieldGlobalId> ApplyFormAction(
      mojom::FormActionType action_type,
      mojom::ActionPersistence action_persistence,
      const FormData& data,
      const url::Origin& triggered_origin,
      const base::flat_map<FieldGlobalId, FieldType>& field_type_map) override;
  void ApplyFieldAction(mojom::FieldActionType action_type,
                        mojom::ActionPersistence action_persistence,
                        const FieldGlobalId& field,
                        const std::u16string& value) override;
  void ExtractForm(
      FormGlobalId form,
      base::OnceCallback<void(AutofillDriver*, const std::optional<FormData>&)>
          response_callback) override;
  void SendAutofillTypePredictionsToRenderer(
      const std::vector<raw_ptr<FormStructure, VectorExperimental>>& forms)
      override;
  void RendererShouldClearFilledSection() override;
  void RendererShouldClearPreviewedForm() override;
  void RendererShouldTriggerSuggestions(
      const FieldGlobalId& field_id,
      AutofillSuggestionTriggerSource trigger_source) override;
  void RendererShouldAcceptDataListSuggestion(
      const FieldGlobalId& field,
      const std::u16string& value) override;
  void TriggerFormExtractionInDriverFrame() override;
  void TriggerFormExtractionInAllFrames(
      base::OnceCallback<void(bool)> form_extraction_finished_callback)
      override;
  void GetFourDigitCombinationsFromDOM(
      base::OnceCallback<void(const std::vector<std::string>&)>
          potential_matches) override;

  void set_autofill_manager_for_testing(
      std::unique_ptr<BrowserAutofillManager> manager) {
    manager_ = std::move(manager);
    manager_observation_.Observe(manager_.get());
  }

  void RendererShouldSetSuggestionAvailability(
      const FieldGlobalId& field,
      mojom::AutofillSuggestionAvailability suggestion_availability) override;
  void PopupHidden() override;
  net::IsolationInfo IsolationInfo() override;

  bool is_processed() const { return processed_; }
  void set_processed(bool processed) { processed_ = processed; }
  web::WebFrame* web_frame() const;

  // Methods routed by AutofillDriverRouter. These are a subset of the methods
  // in mojom::AutofillDriver; that interface is content-specific, but to
  // simplify interaction with the Router, we duplicate some methods (with a few
  // irrelevant args omitted). See
  // components/autofill/content/common/mojom/autofill_driver.mojom
  // for further documentation of each method.
  void AskForValuesToFill(const FormData& form, const FormFieldData& field);
  void DidFillAutofillFormData(const FormData& form, base::TimeTicks timestamp);
  void FormsSeen(const std::vector<FormData>& updated_forms);
  void FormSubmitted(const FormData& form,
                     bool known_success,
                     mojom::SubmissionSource submission_source);
  void TextFieldDidChange(const FormData& form,
                          const FormFieldData& field,
                          base::TimeTicks timestamp);

 private:
  friend AutofillDriverIOSFactory;

  AutofillDriverIOS(web::WebState* web_state,
                    web::WebFrame* web_frame,
                    AutofillClient* client,
                    id<AutofillDriverIOSBridge> bridge,
                    const std::string& app_locale);

  void SetParent(base::WeakPtr<AutofillDriverIOS> parent);

  // Sets `this` as the parent of the frame identified by `token`.
  void SetSelfAsParent(LocalFrameToken token);

  // Only used by the AutofillDriverIOSFactory.
  // Other callers should use FromWebStateAndWebFrame() instead.
  using web::WebFrameUserData<AutofillDriverIOS>::FromWebFrame;

  // AutofillManager::Observer:
  void OnAutofillManagerDestroyed(AutofillManager& manager) override;
  void OnAfterFormsSeen(AutofillManager& manager,
                        base::span<const FormGlobalId> forms) override;

  // The WebState with which this object is associated.
  raw_ptr<web::WebState> web_state_ = nullptr;

  // The id of the WebFrame with which this object is associated.
  // "" if frame messaging is disabled.
  std::string web_frame_id_;

  // A LocalFrameToken containing a value equivalent to `web_frame_id_` if that
  // string is populated with a valid 128-bit hex value, or empty otherwise.
  LocalFrameToken local_frame_token_;

  // The driver of this frame's parent frame, if it is known and valid. Always
  // null for the main (root) frame.
  base::WeakPtr<AutofillDriverIOS> parent_ = nullptr;

  // All RemoteFrameTokens that have ever been dispatched from this frame to
  // a child frame.
  base::flat_set<RemoteFrameToken> known_child_frames_;

  // AutofillDriverIOSBridge instance that is passed in.
  __unsafe_unretained id<AutofillDriverIOSBridge> bridge_;

  // Whether the initial processing has been done (JavaScript observers have
  // been enabled and the forms have been extracted).
  bool processed_ = false;

  // The embedder's AutofillClient instance.
  raw_ref<AutofillClient> client_;

  std::unique_ptr<BrowserAutofillManager> manager_;

  base::ScopedObservation<AutofillManager, AutofillManager::Observer>
      manager_observation_{this};

  base::WeakPtrFactory<AutofillDriverIOS> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_DRIVER_IOS_H_
