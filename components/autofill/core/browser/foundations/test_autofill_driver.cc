// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/foundations/test_autofill_driver.h"

#include <optional>
#include <variant>

#include "base/check_deref.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_manager_test_api.h"

namespace autofill {

TestAutofillDriver::TestAutofillDriver(TestAutofillClient* client)
    : autofill_client_(CHECK_DEREF(client)) {}

TestAutofillDriver::~TestAutofillDriver() {
  autofill_manager_.reset();
}

LocalFrameToken TestAutofillDriver::GetFrameToken() const {
  return frame_token_;
}

std::optional<LocalFrameToken> TestAutofillDriver::Resolve(FrameToken query) {
  if (auto* local_frame_token = std::get_if<LocalFrameToken>(&query)) {
    return *local_frame_token;
  }
  auto it = remote_frame_tokens_.find(std::get<RemoteFrameToken>(query));
  if (it != remote_frame_tokens_.end()) {
    return it->second;
  }
  return std::nullopt;
}

TestAutofillDriver* TestAutofillDriver::GetParent() {
  return parent_;
}

bool TestAutofillDriver::IsActive() const {
  return is_active_;
}

bool TestAutofillDriver::IsEmbedded() const {
  return is_embedded_;
}

TestAutofillClient& TestAutofillDriver::GetAutofillClient() {
  return *autofill_client_;
}

AutofillManager& TestAutofillDriver::GetAutofillManager() {
  return CHECK_DEREF(autofill_manager_);
}

ukm::SourceId TestAutofillDriver::GetPageUkmSourceId() const {
  return const_cast<TestAutofillDriver*>(this)->GetPageUkmSourceId();
}

bool TestAutofillDriver::IsPolicyControlledFeatureAutofillEnabled() const {
  return policy_controlled_feature_autofill_enabled_;
}

bool TestAutofillDriver::IsPolicyControlledFeatureManualTextEnabled() const {
  return policy_controlled_feature_manual_text_enabled_;
}

bool TestAutofillDriver::CanShowAutofillUi() const {
  return true;
}

std::optional<net::IsolationInfo> TestAutofillDriver::GetIsolationInfo() {
  // In AutofillDriverIOS, we always return std::nullopt here. That behavior
  // should be reflected in iOS tests.
#if BUILDFLAG(IS_IOS)
  return std::nullopt;
#else
  return isolation_info_;
#endif
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
    bool force,
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

bool TestAutofillDriver::IsSafeToFill(const FormFieldData& field,
                                      FieldType filled_type,
                                      const url::Origin& main_origin,
                                      const url::Origin& trigger_origin) const {
  // Simplified security model which allows to filter (only) fields from the
  // same origin.
  return field.origin() == trigger_origin;
}

}  // namespace autofill
