// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_autofill_driver.h"

#include "build/build_config.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"

#include "ui/accessibility/ax_tree_id.h"
#include "ui/gfx/geometry/rect_f.h"

namespace autofill {

TestAutofillDriver::TestAutofillDriver()
    : test_shared_loader_factory_(
          base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
              &test_url_loader_factory_)) {}

TestAutofillDriver::~TestAutofillDriver() {}

bool TestAutofillDriver::IsIncognito() const {
  return is_incognito_;
}

bool TestAutofillDriver::IsInMainFrame() const {
  return is_in_main_frame_;
}

ui::AXTreeID TestAutofillDriver::GetAxTreeId() const {
  NOTIMPLEMENTED() << "See https://crbug.com/985933";
  return ui::AXTreeIDUnknown();
}

scoped_refptr<network::SharedURLLoaderFactory>
TestAutofillDriver::GetURLLoaderFactory() {
  return test_shared_loader_factory_;
}

bool TestAutofillDriver::RendererIsAvailable() {
  return true;
}

#if !defined(OS_IOS)
void TestAutofillDriver::ConnectToAuthenticator(
    mojo::PendingReceiver<blink::mojom::InternalAuthenticator> receiver) {}
#endif

void TestAutofillDriver::SendFormDataToRenderer(int query_id,
                                                RendererFormDataAction action,
                                                const FormData& form_data) {
}

void TestAutofillDriver::PropagateAutofillPredictions(
    const std::vector<FormStructure*>& forms) {
}

void TestAutofillDriver::SendAutofillTypePredictionsToRenderer(
    const std::vector<FormStructure*>& forms) {
}

void TestAutofillDriver::RendererShouldAcceptDataListSuggestion(
    const base::string16& value) {
}

void TestAutofillDriver::RendererShouldClearFilledSection() {}

void TestAutofillDriver::RendererShouldClearPreviewedForm() {
}

void TestAutofillDriver::RendererShouldFillFieldWithValue(
    const base::string16& value) {
}

void TestAutofillDriver::RendererShouldPreviewFieldWithValue(
    const base::string16& value) {
}

void TestAutofillDriver::RendererShouldSetSuggestionAvailability(
    const mojom::AutofillState state) {}

void TestAutofillDriver::PopupHidden() {
}

gfx::RectF TestAutofillDriver::TransformBoundingBoxToViewportCoordinates(
    const gfx::RectF& bounding_box) {
  return bounding_box;
}

net::NetworkIsolationKey TestAutofillDriver::NetworkIsolationKey() {
  return network_isolation_key_;
}

void TestAutofillDriver::SetIsIncognito(bool is_incognito) {
  is_incognito_ = is_incognito;
}

void TestAutofillDriver::SetIsInMainFrame(bool is_in_main_frame) {
  is_in_main_frame_ = is_in_main_frame;
}

void TestAutofillDriver::SetNetworkIsolationKey(
    const net::NetworkIsolationKey& network_isolation_key) {
  network_isolation_key_ = network_isolation_key;
}

void TestAutofillDriver::SetSharedURLLoaderFactory(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  test_shared_loader_factory_ = url_loader_factory;
}

}  // namespace autofill
