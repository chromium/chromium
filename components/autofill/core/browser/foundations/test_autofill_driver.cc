// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/foundations/test_autofill_driver.h"

#include "base/check_deref.h"
#include "components/autofill/core/browser/foundations/autofill_manager_test_api.h"

namespace autofill {

TestAutofillDriver::TestAutofillDriver(TestAutofillClient* client)
    : autofill_client_(CHECK_DEREF(client)) {}

TestAutofillDriver::~TestAutofillDriver() = default;

TestAutofillClient& TestAutofillDriver::GetAutofillClient() {
  return *autofill_client_;
}

AutofillManager& TestAutofillDriver::GetAutofillManager() {
  return *autofill_manager_;
}

ukm::SourceId TestAutofillDriver::GetPageUkmSourceId() const {
  return const_cast<TestAutofillDriver*>(this)->GetPageUkmSourceId();
}

void TestAutofillDriver::InitializeUKMSources() {
  GetAutofillClient().GetUkmRecorder()->UpdateSourceURL(ukm_source_id_, url_);
}

ukm::SourceId TestAutofillDriver::GetPageUkmSourceId() {
  if (auto* parent = GetParent()) {
    return parent->GetPageUkmSourceId();
  }
  if (ukm_source_id_ == ukm::kInvalidSourceId) {
    ukm_source_id_ = ukm::UkmRecorder::GetNewSourceID();
    GetAutofillClient().GetUkmRecorder()->UpdateSourceURL(ukm_source_id_, url_);
  }
  return ukm_source_id_;
}

}  // namespace autofill
