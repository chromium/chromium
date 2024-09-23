// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_metrics_recorder.h"

#include <memory>

#include "base/metrics/histogram_macros.h"
#include "components/autofill/core/common/save_password_progress_logger.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "url/gurl.h"

// Shorten the name to spare line breaks. The code provides enough context
// already.
typedef autofill::SavePasswordProgressLogger Logger;

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
    ProvisionalSaveFailure failure,
    const GURL& main_frame_url,
    const GURL& form_origin,
    BrowserSavePasswordProgressLogger* logger) {
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.ProvisionalSaveFailure2", failure,
                            MAX_FAILURE_VALUE);
  ukm_entry_builder_->SetProvisionalSaveFailure(static_cast<int64_t>(failure));

  if (logger) {
    switch (failure) {
      case SAVING_DISABLED:
        logger->LogMessage(Logger::STRING_SAVING_DISABLED);
        break;
      case EMPTY_PASSWORD:
        logger->LogMessage(Logger::STRING_EMPTY_PASSWORD);
        break;
      case MATCHING_NOT_COMPLETE:
        logger->LogMessage(Logger::STRING_MATCHING_NOT_COMPLETE);
        break;
      case NO_MATCHING_FORM:
        logger->LogMessage(Logger::STRING_NO_MATCHING_FORM);
        break;
      case INVALID_FORM:
        logger->LogMessage(Logger::STRING_INVALID_FORM);
        break;
      case SYNC_CREDENTIAL:
        logger->LogMessage(Logger::STRING_SYNC_CREDENTIAL);
        break;
      case SAVING_ON_HTTP_AFTER_HTTPS:
        logger->LogSuccessiveOrigins(
            Logger::STRING_BLOCK_PASSWORD_SAME_ORIGIN_INSECURE_SCHEME,
            main_frame_url.DeprecatedGetOriginAsURL(),
            form_origin.DeprecatedGetOriginAsURL());
        break;
      case MAX_FAILURE_VALUE:
        NOTREACHED_IN_MIGRATION();
        return;
    }
    logger->LogMessage(Logger::STRING_DECISION_DROP);
  }
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
