// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_DRIVER_IOS_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_DRIVER_IOS_H_

#include <string>

#include "base/containers/flat_map.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "ios/web/public/js_messaging/web_frame_user_data.h"
#include "url/origin.h"

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
                          public web::WebFrameUserData<AutofillDriverIOS> {
 public:
  // Returns the AutofillDriverIOS for `web_state` and `web_frame`. Creates the
  // driver if necessary.
  static AutofillDriverIOS* FromWebStateAndWebFrame(web::WebState* web_state,
                                                    web::WebFrame* web_frame);

  ~AutofillDriverIOS() override;

  // AutofillDriver:
  LocalFrameToken GetFrameToken() const override;
  absl::optional<LocalFrameToken> Resolve(FrameToken query) override;
  AutofillDriverIOS* GetParent() override;
  BrowserAutofillManager& GetAutofillManager() override;
  bool IsInActiveFrame() const override;
  bool IsInAnyMainFrame() const override;
  bool IsPrerendering() const override;
  bool HasSharedAutofillPermission() const override;
  bool CanShowAutofillUi() const override;
  bool RendererIsAvailable() override;
  std::vector<FieldGlobalId> ApplyFormAction(
      mojom::ActionType action_type,
      mojom::ActionPersistence action_persistence,
      const FormData& data,
      const url::Origin& triggered_origin,
      const base::flat_map<FieldGlobalId, ServerFieldType>& field_type_map)
      override;
  void ApplyFieldAction(mojom::ActionPersistence action_persistence,
                        mojom::TextReplacement text_replacement,
                        const FieldGlobalId& field,
                        const std::u16string& value) override;
  void ExtractForm(
      FormGlobalId form,
      base::OnceCallback<void(AutofillDriver*, const std::optional<FormData>&)>
          response_callback) override;
  void HandleParsedForms(const std::vector<FormData>& forms) override;
  void SendAutofillTypePredictionsToRenderer(
      const std::vector<FormStructure*>& forms) override;
  void RendererShouldClearFilledSection() override;
  void RendererShouldClearPreviewedForm() override;
  void RendererShouldTriggerSuggestions(
      const FieldGlobalId& field_id,
      AutofillSuggestionTriggerSource trigger_source) override;
  void RendererShouldAcceptDataListSuggestion(
      const FieldGlobalId& field,
      const std::u16string& value) override;
  void SendFieldsEligibleForManualFillingToRenderer(
      const std::vector<FieldGlobalId>& fields) override;
  void TriggerFormExtractionInDriverFrame() override;
  void TriggerFormExtractionInAllFrames(
      base::OnceCallback<void(bool)> form_extraction_finished_callback)
      override;
  void GetFourDigitCombinationsFromDOM(
      base::OnceCallback<void(const std::vector<std::string>&)>
          potential_matches) override;

  AutofillClient* client() { return client_; }

  void set_autofill_manager_for_testing(
      std::unique_ptr<BrowserAutofillManager> browser_autofill_manager) {
    browser_autofill_manager_ = std::move(browser_autofill_manager);
  }

  void RendererShouldSetSuggestionAvailability(
      const FieldGlobalId& field,
      const mojom::AutofillState state) override;
  void PopupHidden() override;
  net::IsolationInfo IsolationInfo() override;

  bool is_processed() const { return processed_; }
  void set_processed(bool processed) { processed_ = processed; }
  web::WebFrame* web_frame() const;

 private:
  friend AutofillDriverIOSFactory;

  AutofillDriverIOS(web::WebState* web_state,
                    web::WebFrame* web_frame,
                    AutofillClient* client,
                    id<AutofillDriverIOSBridge> bridge,
                    const std::string& app_locale);

  // Only used by the AutofillDriverIOSFactory.
  // Other callers should use FromWebStateAndWebFrame() instead.
  using web::WebFrameUserData<AutofillDriverIOS>::FromWebFrame;

  // The WebState with which this object is associated.
  web::WebState* web_state_ = nullptr;

  // The id of the WebFrame with which this object is associated.
  // "" if frame messaging is disabled.
  std::string web_frame_id_;

  // AutofillDriverIOSBridge instance that is passed in.
  __unsafe_unretained id<AutofillDriverIOSBridge> bridge_;

  // Whether the initial processing has been done (JavaScript observers have
  // been enabled and the forms have been extracted).
  bool processed_ = false;

  // The embedder's AutofillClient instance.
  AutofillClient* client_;

  // BrowserAutofillManager instance via which this object drives the shared
  // Autofill code.
  std::unique_ptr<BrowserAutofillManager> browser_autofill_manager_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_DRIVER_IOS_H_
