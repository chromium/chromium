// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_DRIVER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_DRIVER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
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
  ui::AXTreeID GetAxTreeId() const override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  bool RendererIsAvailable() override;
#if !defined(OS_IOS)
  void ConnectToAuthenticator(
      mojo::PendingReceiver<blink::mojom::InternalAuthenticator> receiver)
      override;
#endif
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
  void RendererShouldSetSuggestionAvailability(
      const mojom::AutofillState state) override;
  void PopupHidden() override;
  gfx::RectF TransformBoundingBoxToViewportCoordinates(
      const gfx::RectF& bounding_box) override;
  net::NetworkIsolationKey NetworkIsolationKey() override;

  // Methods unique to TestAutofillDriver that tests can use to specialize
  // functionality.

  void SetIsIncognito(bool is_incognito);
  void SetIsInMainFrame(bool is_in_main_frame);
  void SetNetworkIsolationKey(
      const net::NetworkIsolationKey& network_isolation_key);

  void SetSharedURLLoaderFactory(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

 private:
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  bool is_incognito_ = false;
  bool is_in_main_frame_ = false;
  net::NetworkIsolationKey network_isolation_key_;

  DISALLOW_COPY_AND_ASSIGN(TestAutofillDriver);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_DRIVER_H_
