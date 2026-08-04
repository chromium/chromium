// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/foundations/test_autofill_driver.h"

#include "base/check_deref.h"
#include "base/task/sequenced_task_runner.h"
#include "components/autofill/core/browser/form_structure.h"
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

void TestAutofillDriver::TriggerFormExtractionInAllFrames(
    base::OnceCallback<void(bool)> form_extraction_finished_callback) {
  std::vector<FormData> forms;
  GetAutofillManager().ForEachCachedForm(
      [&forms](const FormStructure& form_structure) {
        forms.push_back(form_structure.ToFormData());
      });
  GetAutofillManager().OnFormsSeen(forms, /*removed_form_ids=*/{},
                                   AutofillManagerTestApi::pass_key());
  if (form_extraction_finished_callback) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(form_extraction_finished_callback), true));
  }
}

}  // namespace autofill
