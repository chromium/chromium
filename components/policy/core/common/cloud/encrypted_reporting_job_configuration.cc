// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/encrypted_reporting_job_configuration.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "net/base/backoff_entry.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace policy {

namespace {

// EncryptedReportingJobConfiguration strings
constexpr char kEncryptedRecordListKey[] = "encryptedRecord";
constexpr char kSequenceInformationKey[] = "sequenceInformation";
constexpr char kSequenceId[] = "sequencingId";
constexpr char kGenerationId[] = "generationId";
constexpr char kPriority[] = "priority";
constexpr char kAttachEncryptionSettingsKey[] = "attachEncryptionSettings";
constexpr char kDeviceKey[] = "device";
constexpr char kBrowserKey[] = "browser";

// Generate new backoff entry.
std::unique_ptr<::net::BackoffEntry> GetBackoffEntry(
    ::reporting::Priority priority) {
  // Retry policy for SECURITY queue.
  static const ::net::BackoffEntry::Policy kSecurityUploadBackoffPolicy = {
      // Number of initial errors to ignore before applying
      // exponential back-off rules.
      /*num_errors_to_ignore=*/0,

      // Initial delay is 10 seconds.
      /*initial_delay_ms=*/10 * 1000,

      // Factor by which the waiting time will be multiplied.
      /*multiply_factor=*/2,

      // Fuzzing percentage.
      /*jitter_factor=*/0.1,

      // Maximum delay is 1 minute.
      /*maximum_backoff_ms=*/1 * 60 * 1000,

      // It's up to the caller to reset the backoff time.
      /*entry_lifetime_ms=*/-1,

      /*always_use_initial_delay=*/true,
  };
  // Retry policy for all other queues, including initial key delivery.
  static const ::net::BackoffEntry::Policy kDefaultUploadBackoffPolicy = {
      // Number of initial errors to ignore before applying
      // exponential back-off rules.
      /*num_errors_to_ignore=*/0,

      // Initial delay is 10 seconds.
      /*initial_delay_ms=*/10 * 1000,

      // Factor by which the waiting time will be multiplied.
      /*multiply_factor=*/2,

      // Fuzzing percentage.
      /*jitter_factor=*/0.1,

      // Maximum delay is 24 hours.
      /*maximum_backoff_ms=*/24 * 60 * 60 * 1000,

      // It's up to the caller to reset the backoff time.
      /*entry_lifetime_ms=*/-1,

      /*always_use_initial_delay=*/true,
  };
  // Maximum backoff is set per priority. Current proposal is to set SECURITY
  // events to be backed off only slightly: max delay is set to 1 minute.
  // For all other priorities max delay is set to 24 hours.
  auto backoff_entry = std::make_unique<::net::BackoffEntry>(
      priority == ::reporting::SECURITY ? &kSecurityUploadBackoffPolicy
                                        : &kDefaultUploadBackoffPolicy);
  return backoff_entry;
}

// State of single priority queue uploads.
// It is a singleton, protected implicitly by the fact that all relevant
// EncryptedReportingJobConfiguration actions are called on the sequenced task
// runner.
struct UploadState {
  // Highest sequence id that has been posted for upload.
  int64_t last_sequence_id;
  // Generation id that has been posted for upload.
  int64_t last_generation_id;

  // Time when the next request will be allowed.
  // This is essentially the cache value of the backoff->GetReleaseTime().
  // When the time is reached, one request is allowed, backoff is updated as if
  // the request failed, and the new release time is cached.
  base::TimeTicks earliest_retry_timestamp;

  // Current backoff entry for this prioririty.
  std::unique_ptr<::net::BackoffEntry> backoff_entry;
};
// Map of all the queues states.
using UploadStateMap = base::flat_map<::reporting::Priority, UploadState>;

UploadStateMap* state_map() {
  static base::NoDestructor<UploadStateMap> map;
  return map.get();
}

UploadState* AccessState(::reporting::Priority priority,
                         int64_t generation_id,
                         int64_t sequence_id) {
  auto state_it = state_map()->find(priority);
  if (state_it == state_map()->end() ||
      state_it->second.last_generation_id != generation_id) {
    // This priority pops up for the first time or (rare case) generation has
    // changed. Record new state and allow upload.
    state_it = state_map()
                   ->insert_or_assign(
                       priority,
                       UploadState{.last_sequence_id = sequence_id,
                                   .last_generation_id = generation_id,
                                   .backoff_entry = GetBackoffEntry(priority)})
                   .first;
    state_it->second.earliest_retry_timestamp =
        state_it->second.backoff_entry->GetReleaseTime();
  }
  return &state_it->second;
}

}  // namespace

EncryptedReportingJobConfiguration::EncryptedReportingJobConfiguration(
    scoped_refptr<network::SharedURLLoaderFactory> factory,
    DMAuth auth_data,
    const std::string& server_url,
    base::Value::Dict merging_payload,
    const std::string& dm_token,
    const std::string& client_id,
    UploadCompleteCallback complete_cb)
    : ReportingJobConfigurationBase(TYPE_UPLOAD_ENCRYPTED_REPORT,
                                    factory,
                                    std::move(auth_data),
                                    server_url,
                                    std::move(complete_cb)) {
  // Init common payload fields.
  // TODO(b/237809917): Init using `InitializePayloadWithoutDeviceInfo` when
  // backend is ready to support unmanaged devices.
  InitializePayloadWithDeviceInfo(dm_token, client_id);
  // Merge it into the base class payload.
  payload_.Merge(std::move(merging_payload));
  // Retrieve priorities and figure out maximum sequence id for each.
  // Payload is expected to be correctly formed, any malformed piece is ignored.
  // TODO(b/214040103): if batching is enabled, multiple priorities may be
  // found. Before that, each payload can only have no more than one, and the
  // highest sequence id comes from the last record.
  // TODO(b/232455728): if test_request_payload is moved to components/
  // we would be able to use it here.
  const auto* const encrypted_record_list =
      payload_.FindList(kEncryptedRecordListKey);
  // If there are no records, assume UNDEFINED priority and seq_id = -1.
  priority_ = ::reporting::UNDEFINED_PRIORITY;
  generation_id_ = -1;
  sequence_id_ = -1;
  if (encrypted_record_list != nullptr && !encrypted_record_list->empty()) {
    const auto sequence_information_it =
        std::prev(encrypted_record_list->cend());
    const auto* const sequence_information =
        sequence_information_it->GetDict().FindDict(kSequenceInformationKey);
    if (sequence_information != nullptr) {
      const auto maybe_priority = sequence_information->FindInt(kPriority);
      auto* const generation_id_ptr =
          sequence_information->FindString(kGenerationId);
      auto* const sequence_id_ptr =
          sequence_information->FindString(kSequenceId);
      if (maybe_priority.has_value() &&
          ::reporting::Priority_IsValid(maybe_priority.value())) {
        priority_ = static_cast<::reporting::Priority>(maybe_priority.value());
      }
      if (generation_id_ptr != nullptr) {
        base::StringToInt64(*generation_id_ptr, &generation_id_);
      }
      if (sequence_id_ptr != nullptr) {
        base::StringToInt64(*sequence_id_ptr, &sequence_id_);
      }
    }
  }
}

EncryptedReportingJobConfiguration::~EncryptedReportingJobConfiguration() {
  if (!callback_.is_null()) {
    // The job either wasn't tried, or failed in some unhandled way. Report
    // failure to the callback.
    std::move(callback_).Run(/*job=*/nullptr,
                             DeviceManagementStatus::DM_STATUS_REQUEST_FAILED,
                             /*response_code=*/418,
                             /*response_body=*/absl::nullopt);
  }
}

void EncryptedReportingJobConfiguration::UpdatePayloadBeforeGetInternal() {
  for (auto it = payload_.begin(); it != payload_.end();) {
    const auto& [key, value] = *it;
    if (!base::Contains(GetTopLevelKeyAllowList(), key)) {
      it = payload_.erase(it);
      continue;
    }
    ++it;
  }
}

void EncryptedReportingJobConfiguration::UpdateContext(
    base::Value::Dict context) {
  context_ = std::move(context);
}

base::TimeDelta EncryptedReportingJobConfiguration::WhenIsAllowedToProceed()
    const {
  // Now pick up the state.
  const auto* const state =
      AccessState(priority_, generation_id_, sequence_id_);
  // Use and update previously recorded state, base upload decision on it.
  if (state->last_sequence_id > sequence_id_) {
    // Sequence id decreased, the upload is outdated, reject it forever.
    return base::TimeDelta::Max();
  }
  if (state->last_sequence_id < sequence_id_) {
    // Sequence id increased, keep validating.
    switch (priority_) {
      case ::reporting::SECURITY:
        // For SECURITY events the request is allowed.
        return base::TimeDelta();  // 0 - allowed right away.
      default: {
        // For all other priorities we will act like in case of request’s
        // last_sequence_id is == last_sequence_id above - observing the
        // backoff time expiration.
      }
    }
  }
  // Allow upload only if earliest retry time has passed.
  // Return delta till the allowed time - if positive, upload is going to be
  // rejected.
  return state->earliest_retry_timestamp -
         state->backoff_entry->GetTimeTicksNow();
}

void EncryptedReportingJobConfiguration::CancelNotAllowedJob() {
  std::move(callback_).Run(
      /*job=*/nullptr, DeviceManagementStatus::DM_STATUS_REQUEST_FAILED,
      /*response_code=*/DeviceManagementService::kTooManyRequests,
      /*response_body=*/absl::nullopt);
}

void EncryptedReportingJobConfiguration::AccountForAllowedJob() {
  auto* const state = AccessState(priority_, generation_id_, sequence_id_);
  // Update state to reflect highest sequence_id_ (we never allow upload with
  // lower sequence_id_).
  state->last_sequence_id = sequence_id_;
  // Calculate delay as exponential backoff (based on the retry_count).
  // Update backoff under assumption that this request fails.
  // If it is responded successfully, we will reset it.
  state->backoff_entry->InformOfRequest(/*succeeded=*/false);
  state->earliest_retry_timestamp = state->backoff_entry->GetReleaseTime();
}

DeviceManagementService::Job::RetryMethod
EncryptedReportingJobConfiguration::ShouldRetry(
    int response_code,
    const std::string& response_body) {
  // Do not retry on the Job level - ERP has its own retry mechanism.
  return DeviceManagementService::Job::NO_RETRY;
}

void EncryptedReportingJobConfiguration::OnURLLoadComplete(
    DeviceManagementService::Job* job,
    int net_error,
    int response_code,
    const std::string& response_body) {
  // Analyze the net error and update upload state for possible future retries.
  auto* const state = AccessState(priority_, generation_id_, sequence_id_);
  if (net_error != ::net::OK) {
    // Network error
  } else if (response_code >= 400 && response_code <= 499 &&
             response_code != 409 /* Overlapping seq_id ranges detected */) {
    // Permanent error code returned by server, impose artificial 24h backoff.
    state->backoff_entry->SetCustomReleaseTime(
        state->backoff_entry->GetTimeTicksNow() + base::Days(1));
  }
  // For all other cases keep the currently set retry time.
  // In case of success, inform backoff entry about that.
  if (net_error == ::net::OK &&
      response_code == DeviceManagementService::kSuccess) {
    state->backoff_entry->InformOfRequest(/*succeeded=*/true);
  }
  // Cache earliest retry time based on the current backoff entry.
  state->earliest_retry_timestamp = state->backoff_entry->GetReleaseTime();

  // Then deliver response and status by making a call to the base class.
  ReportingJobConfigurationBase::OnURLLoadComplete(
      job, net_error, response_code, response_body);
}

std::string EncryptedReportingJobConfiguration::GetUmaString() const {
  return "Browser.ERP.";
}

std::set<std::string>
EncryptedReportingJobConfiguration::GetTopLevelKeyAllowList() {
  static std::set<std::string> kTopLevelKeyAllowList{
      kEncryptedRecordListKey, kAttachEncryptionSettingsKey, kDeviceKey,
      kBrowserKey};
  return kTopLevelKeyAllowList;
}

// static
void EncryptedReportingJobConfiguration::ResetUploadsStateForTest() {
  state_map()->clear();
}

}  // namespace policy
