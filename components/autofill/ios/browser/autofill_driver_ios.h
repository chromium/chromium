// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_AUTOFILL_DRIVER_IOS_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_AUTOFILL_DRIVER_IOS_H_

#include <string>

#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/autofill_external_delegate.h"
#include "components/autofill/core/browser/autofill_manager.h"

namespace web {
class WebFrame;
class WebState;
}

@protocol AutofillDriverIOSBridge;

namespace autofill {

// Class that drives autofill flow on iOS. There is one instance per
// WebContents.
class AutofillDriverIOS : public AutofillDriver {
 public:
  ~AutofillDriverIOS() override;

  static void PrepareForWebStateWebFrameAndDelegate(
      web::WebState* web_state,
      AutofillClient* client,
      id<AutofillDriverIOSBridge> bridge,
      const std::string& app_locale,
      AutofillManager::AutofillDownloadManagerState enable_download_manager);

  static AutofillDriverIOS* FromWebStateAndWebFrame(web::WebState* web_state,
                                                    web::WebFrame* web_frame);

  // AutofillDriver:
  bool IsIncognito() const override;
  bool IsInMainFrame() const override;
  net::URLRequestContextGetter* GetURLRequestContext() override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  bool RendererIsAvailable() override;
  void SendFormDataToRenderer(int query_id,
                              RendererFormDataAction action,
                              const FormData& data) override;
  void PropagateAutofillPredictions(
      const std::vector<autofill::FormStructure*>& forms) override;
  void SendAutofillTypePredictionsToRenderer(
      const std::vector<FormStructure*>& forms) override;
  void RendererShouldClearFilledSection() override;
  void RendererShouldClearPreviewedForm() override;
  void RendererShouldAcceptDataListSuggestion(
      const base::string16& value) override;
  void DidInteractWithCreditCardForm() override;

  AutofillManager* autofill_manager() { return &autofill_manager_; }

  void RendererShouldFillFieldWithValue(const base::string16& value) override;
  void RendererShouldPreviewFieldWithValue(
      const base::string16& value) override;
  void PopupHidden() override;
  gfx::RectF TransformBoundingBoxToViewportCoordinates(
      const gfx::RectF& bounding_box) override;

  bool is_processed() const { return processed_; }
  void set_processed(bool processed) { processed_ = processed; };

 protected:
  AutofillDriverIOS(
      web::WebState* web_state,
      web::WebFrame* web_frame,
      AutofillClient* client,
      id<AutofillDriverIOSBridge> bridge,
      const std::string& app_locale,
      AutofillManager::AutofillDownloadManagerState enable_download_manager);

 private:
  // The WebState with which this object is associated.
  web::WebState* web_state_ = nullptr;

  // The WebState with which this object is associated.
  // nullptr if frame messaging is disabled.
  web::WebFrame* web_frame_ = nullptr;

  // AutofillDriverIOSBridge instance that is passed in.
  __unsafe_unretained id<AutofillDriverIOSBridge> bridge_;

  // Whether the initial processing has been done (JavaScript observers have
  // been enabled and the forms have been extracted).
  bool processed_ = false;

  // AutofillManager instance via which this object drives the shared Autofill
  // code.
  AutofillManager autofill_manager_;
  // AutofillExternalDelegate instance that is passed to the AutofillManager.
  AutofillExternalDelegate autofill_external_delegate_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_AUTOFILL_DRIVER_IOS_H_
