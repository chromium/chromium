// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_DRIVER_IOS_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_DRIVER_IOS_H_

#include <string>

#include "base/containers/flat_map.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/autofill_external_delegate.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "url/origin.h"

namespace web {
class WebFrame;
class WebState;
}

@protocol AutofillDriverIOSBridge;

namespace autofill {

// AutofillDriverIOS drives the Autofill flow in the browser process based
// on communication from JavaScript and from the external world.
//
// AutofillDriverIOS communicates with an AutofillDriverIOSBridge, which in
// Chrome is implemented by AutofillAgent, and a BrowserAutofillManager.
//
// AutofillDriverIOS is associated with exactly one WebFrame, but its lifecycle
// does *not* follow the life of that WebFrame precisely: an AutofillDriverIOS
// survives the associated WebFrame and is destroyed only on destruction of the
// associated WebState. This lifetime extension is done via a ref-counted
// pointer in AutofillAgent.
//
// TODO(crbug.com/892612, crbug.com/1394786): Remove this workaround once life
// cycle of AutofillDownloadManager is fixed.
class AutofillDriverIOS : public AutofillDriver {
 public:
  ~AutofillDriverIOS() override;

  static void PrepareForWebStateWebFrameAndDelegate(
      web::WebState* web_state,
      AutofillClient* client,
      id<AutofillDriverIOSBridge> bridge,
      const std::string& app_locale,
      AutofillManager::EnableDownloadManager enable_download_manager);

  static AutofillDriverIOS* FromWebStateAndWebFrame(web::WebState* web_state,
                                                    web::WebFrame* web_frame);

  // AutofillDriver:
  bool IsInActiveFrame() const override;
  bool IsInAnyMainFrame() const override;
  bool IsPrerendering() const override;
  bool CanShowAutofillUi() const override;
  ui::AXTreeID GetAxTreeId() const override;
  bool RendererIsAvailable() override;
  std::vector<FieldGlobalId> FillOrPreviewForm(
      mojom::RendererFormDataAction action,
      const FormData& data,
      const url::Origin& triggered_origin,
      const base::flat_map<FieldGlobalId, ServerFieldType>& field_type_map)
      override;
  void HandleParsedForms(const std::vector<FormData>& forms) override;
  void SendAutofillTypePredictionsToRenderer(
      const std::vector<FormStructure*>& forms) override;
  void RendererShouldClearFilledSection() override;
  void RendererShouldClearPreviewedForm() override;
  void RendererShouldAcceptDataListSuggestion(
      const FieldGlobalId& field,
      const std::u16string& value) override;
  void SendFieldsEligibleForManualFillingToRenderer(
      const std::vector<FieldGlobalId>& fields) override;
  void SetShouldSuppressKeyboard(bool suppress) override;
  void TriggerReparseInAllFrames() override;

  AutofillClient* client() { return client_; }

  void set_autofill_manager_for_testing(
      std::unique_ptr<BrowserAutofillManager> browser_autofill_manager) {
    browser_autofill_manager_ = std::move(browser_autofill_manager);
  }

  BrowserAutofillManager* autofill_manager() {
    return browser_autofill_manager_.get();
  }

  void RendererShouldFillFieldWithValue(const FieldGlobalId& field,
                                        const std::u16string& value) override;
  void RendererShouldPreviewFieldWithValue(
      const FieldGlobalId& field,
      const std::u16string& value) override;
  void RendererShouldSetSuggestionAvailability(
      const FieldGlobalId& field,
      const mojom::AutofillState state) override;
  void PopupHidden() override;
  net::IsolationInfo IsolationInfo() override;

  bool is_processed() const { return processed_; }
  void set_processed(bool processed) { processed_ = processed; }
  web::WebFrame* web_frame();

 protected:
  AutofillDriverIOS(
      web::WebState* web_state,
      web::WebFrame* web_frame,
      AutofillClient* client,
      id<AutofillDriverIOSBridge> bridge,
      const std::string& app_locale,
      AutofillManager::EnableDownloadManager enable_download_manager);

 private:
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
