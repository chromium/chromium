// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_metrics_recorder.h"

#include <memory>

#include "base/metrics/histogram_macros.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "url/gurl.h"

namespace password_manager {

PasswordManagerMetricsRecorder::PasswordManagerMetricsRecorder(
    ukm::SourceId source_id)
    : ukm_entry_builder_(
          std::make_unique<ukm::builders::PageWithPassword>(source_id)) {}

PasswordManagerMetricsRecorder::PasswordManagerMetricsRecorder(
    PasswordManagerMetricsRecorder&& that) noexcept = default;

PasswordManagerMetricsRecorder::~PasswordManagerMetricsRecorder() {
  if (user_modified_password_field_) {
    ukm_entry_builder_->SetUserModifiedPasswordField(1);
  }
  if (form_manager_availability_ != FormManagerAvailable::kNotSet) {
    ukm_entry_builder_->SetFormManagerAvailable(
        static_cast<int64_t>(form_manager_availability_));
  }
  ukm_entry_builder_->Record(ukm::UkmRecorder::Get());
}

PasswordManagerMetricsRecorder& PasswordManagerMetricsRecorder::operator=(
    PasswordManagerMetricsRecorder&& that) = default;

void PasswordManagerMetricsRecorder::RecordUserModifiedPasswordField() {
  user_modified_password_field_ = true;
}

void PasswordManagerMetricsRecorder::RecordProvisionalSaveFailure(
    ProvisionalSaveFailure failure) {
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.ProvisionalSaveFailure2", failure,
                            MAX_FAILURE_VALUE);
  ukm_entry_builder_->SetProvisionalSaveFailure(static_cast<int64_t>(failure));
}

void PasswordManagerMetricsRecorder::RecordFormManagerAvailable(
    FormManagerAvailable availability) {
  form_manager_availability_ = availability;
}

void PasswordManagerMetricsRecorder::RecordPageLevelUserAction(
    PasswordManagerMetricsRecorder::PageLevelUserAction action) {
  ukm_entry_builder_->SetPageLevelUserAction(static_cast<int64_t>(action));
}

}  // namespace password_manager
