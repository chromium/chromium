// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_DRIVER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_DRIVER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "services/network/test/test_url_loader_factory.h"

namespace autofill {

// This class is only for easier writing of tests.
class TestAutofillDriver : public AutofillDriver {
 public:
  TestAutofillDriver();
  ~TestAutofillDriver() override;

  // AutofillDriver implementation overrides.
  bool IsIncognito() const override;
  bool IsInMainFrame() const override;
  // Returns the value passed in to the last call to |SetURLRequestContext()|
  // or NULL if that method has never been called.
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
  void RendererShouldAcceptDataListSuggestion(
      const base::string16& value) override;
  void RendererShouldClearFilledSection() override;
  void RendererShouldClearPreviewedForm() override;
  void RendererShouldFillFieldWithValue(const base::string16& value) override;
  void RendererShouldPreviewFieldWithValue(
      const base::string16& value) override;
  void PopupHidden() override;
  gfx::RectF TransformBoundingBoxToViewportCoordinates(
      const gfx::RectF& bounding_box) override;
  void DidInteractWithCreditCardForm() override;

  // Methods unique to TestAutofillDriver that tests can use to specialize
  // functionality.

  void ClearDidInteractWithCreditCardForm();

  bool GetDidInteractWithCreditCardForm() const;

  void SetIsIncognito(bool is_incognito);
  void SetIsInMainFrame(bool is_in_main_frame);

  // Sets the URL request context for this instance. |url_request_context|
  // should outlive this instance.
  void SetURLRequestContext(net::URLRequestContextGetter* url_request_context);
  void SetSharedURLLoaderFactory(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

 private:
  net::URLRequestContextGetter* url_request_context_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  bool is_incognito_ = false;
  bool is_in_main_frame_ = false;
  bool did_interact_with_credit_card_form_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestAutofillDriver);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_DRIVER_H_
