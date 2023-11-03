// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/ukm_test_helper.h"

#include <algorithm>
#include <string>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/run_loop.h"
#include "components/metrics/log_decoder.h"
#include "components/metrics/metrics_logs_event_manager.h"
#include "components/metrics/unsent_log_store.h"
#include "ukm_test_helper.h"

namespace ukm {

UkmTestHelper::UkmTestHelper(UkmService* ukm_service)
    : ukm_service_(ukm_service) {}

bool UkmTestHelper::IsExtensionRecordingEnabled() const {
  return ukm_service_ ? ukm_service_->recording_enabled(EXTENSIONS) : false;
}

bool UkmTestHelper::IsRecordingEnabled() const {
  return ukm_service_ ? ukm_service_->recording_enabled() : false;
}

bool UkmTestHelper::IsReportUserNoisedUserBirthYearAndGenderEnabled() {
  return base::FeatureList::IsEnabled(
      ukm::kReportUserNoisedUserBirthYearAndGender);
}

uint64_t UkmTestHelper::GetClientId() {
  return ukm_service_->client_id_;
}

std::unique_ptr<Report> UkmTestHelper::GetUkmReport() {
  if (!HasUnsentLogs()) {
    return nullptr;
  }

  metrics::UnsentLogStore* log_store =
      ukm_service_->reporting_service_.ukm_log_store();
  if (log_store->has_staged_log()) {
    // For testing purposes, we examine the content of a staged log without
    // ever sending the log, so discard any previously staged log.
    log_store->DiscardStagedLog();
  }

  log_store->StageNextLog();
  if (!log_store->has_staged_log()) {
    return nullptr;
  }

  std::unique_ptr<ukm::Report> report = std::make_unique<ukm::Report>();
  if (!metrics::DecodeLogDataToProto(log_store->staged_log(), report.get())) {
    return nullptr;
  }

  return report;
}

UkmSource* UkmTestHelper::GetSource(SourceId source_id) {
  if (!ukm_service_) {
    return nullptr;
  }

  auto it = ukm_service_->sources().find(source_id);
  return it == ukm_service_->sources().end() ? nullptr : it->second.get();
}

bool UkmTestHelper::HasSource(SourceId source_id) {
  return ukm_service_ && base::Contains(ukm_service_->sources(), source_id);
}

bool UkmTestHelper::IsSourceObsolete(SourceId source_id) {
  return ukm_service_ &&
         base::Contains(ukm_service_->recordings_.obsolete_source_ids,
                        source_id);
}

void UkmTestHelper::RecordSourceForTesting(SourceId source_id) {
  if (ukm_service_) {
    ukm_service_->UpdateSourceURL(source_id, GURL("http://example.com"));
  }
}

void UkmTestHelper::BuildAndStoreLog() {
  // Wait for initialization to complete before flushing.
  base::RunLoop run_loop;
  ukm_service_->SetInitializationCompleteCallbackForTesting(
      run_loop.QuitClosure());
  run_loop.Run();

  ukm_service_->Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);
}

bool UkmTestHelper::HasUnsentLogs() {
  return ukm_service_ &&
         ukm_service_->reporting_service_.ukm_log_store()->has_unsent_logs();
}

void UkmTestHelper::SetMsbbConsent() {
  DCHECK(ukm_service_);
  ukm_service_->UpdateRecording({ukm::MSBB});
}

void UkmTestHelper::PurgeData() {
  DCHECK(ukm_service_);
  ukm_service_->Purge();
}

}  // namespace ukm
