// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_autofill_driver.h"

#include "build/build_config.h"

#include "ui/accessibility/ax_tree_id.h"
#include "ui/gfx/geometry/rect_f.h"

namespace autofill {

TestAutofillDriver::TestAutofillDriver()
#if !BUILDFLAG(IS_IOS)
    : ContentAutofillDriver(/*render_frame_host=*/nullptr,
                            /*autofill_router=*/nullptr)
#endif
{
}

TestAutofillDriver::~TestAutofillDriver() = default;

bool TestAutofillDriver::IsInActiveFrame() const {
  return is_in_active_frame_;
}

bool TestAutofillDriver::IsInAnyMainFrame() const {
  return is_in_any_main_frame_;
}

bool TestAutofillDriver::IsPrerendering() const {
  return false;
}

bool TestAutofillDriver::CanShowAutofillUi() const {
  return true;
}

ui::AXTreeID TestAutofillDriver::GetAxTreeId() const {
  NOTIMPLEMENTED() << "See https://crbug.com/985933";
  return ui::AXTreeIDUnknown();
}

bool TestAutofillDriver::RendererIsAvailable() {
  return true;
}

std::vector<FieldGlobalId> TestAutofillDriver::FillOrPreviewForm(
    mojom::RendererFormDataAction action,
    const FormData& form_data,
    const url::Origin& triggered_origin,
    const base::flat_map<FieldGlobalId, ServerFieldType>& field_type_map) {
  std::vector<FieldGlobalId> result;
  for (const auto& [id, type] : field_type_map) {
    if (!field_type_map_filter_ ||
        field_type_map_filter_.Run(triggered_origin, id, type)) {
      result.push_back(id);
    }
  }
  return result;
}

net::IsolationInfo TestAutofillDriver::IsolationInfo() {
  return isolation_info_;
}

void TestAutofillDriver::SetIsInActiveFrame(bool is_in_active_frame) {
  is_in_active_frame_ = is_in_active_frame;
}

void TestAutofillDriver::SetIsInAnyMainFrame(bool is_in_any_main_frame) {
  is_in_any_main_frame_ = is_in_any_main_frame;
}

void TestAutofillDriver::SetIsolationInfo(
    const net::IsolationInfo& isolation_info) {
  isolation_info_ = isolation_info;
}

void TestAutofillDriver::SetFieldTypeMapFilter(
    base::RepeatingCallback<
        bool(const url::Origin&, FieldGlobalId, ServerFieldType)> callback) {
  field_type_map_filter_ = callback;
}

#if !BUILDFLAG(IS_IOS)
void TestAutofillDriver::SetAuthenticator(
    webauthn::InternalAuthenticator* authenticator_) {
  test_authenticator_.reset(authenticator_);
}
#endif

}  // namespace autofill
