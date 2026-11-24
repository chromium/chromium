// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/unsent_log_store.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_base.h"
#include "base/numerics/clamped_math.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/string_view_util.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "components/metrics/log_store.h"
#include "components/metrics/metrics_features.h"
#include "components/metrics/unsent_log_store_metrics.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "crypto/hmac.h"
#include "crypto/obsolete/sha1.h"
#include "third_party/zlib/google/compression_utils.h"

namespace metrics {

namespace {

const char kLogHashKey[] = "hash";
const char kLogSignatureKey[] = "signature";
const char kLogTimestampKey[] = "timestamp";
const char kLogDataKey[] = "data";
const char kLogUnsentCountKey[] = "unsent_samples_count";
const char kLogSentCountKey[] = "sent_samples_count";
const char kLogPersistedSizeInKbKey[] = "unsent_persisted_size_in_kb";
const char kLogUserIdKey[] = "user_id";
const char kLogSourceType[] = "type";
const char kLogSamplesCountKey[] = "samples_count";

std::string EncodeToBase64(const std::string& to_convert) {
  DCHECK(to_convert.data());
  return base::Base64Encode(to_convert);
}

std::string DecodeFromBase64(const std::string& to_convert) {
  std::string result;
  base::Base64Decode(to_convert, &result);
  return result;
}

// Used to write unsent logs to prefs.
class LogsPrefWriter {
 public:
  // Create a writer that will write unsent logs to |list_value|. |list_value|
  // should be a base::ListValue representing a pref. Clears the contents of
  // |list_value|.
  explicit LogsPrefWriter(base::ListValue* list_value)
      : list_value_(list_value) {
    DCHECK(list_value);
    list_value->clear();
  }

  LogsPrefWriter(const LogsPrefWriter&) = delete;
  LogsPrefWriter& operator=(const LogsPrefWriter&) = delete;

  ~LogsPrefWriter() = default;

  // Persists |log| by appending it to |list_value_|.
  void WriteLogEntry(UnsentLogStore::LogInfo* log) {
    base::DictValue dict_value;
    dict_value.Set(kLogHashKey, EncodeToBase64(log->hash));
    dict_value.Set(kLogSignatureKey, EncodeToBase64(log->signature));
    dict_value.Set(kLogDataKey, EncodeToBase64(log->compressed_log_data));
    dict_value.Set(kLogTimestampKey, log->timestamp);
    if (log->log_metadata.log_source_type.has_value()) {
      dict_value.Set(
          kLogSourceType,
          static_cast<int>(log->log_metadata.log_source_type.value()));
    }
    auto user_id = log->log_metadata.user_id;
    if (user_id.has_value()) {
      dict_value.Set(kLogUserIdKey,
                     EncodeToBase64(base::NumberToString(user_id.value())));
    }
    auto samples_count = log->log_metadata.samples_count;
    if (samples_count.has_value()) {
      dict_value.Set(kLogSamplesCountKey, samples_count.value());
      unsent_samples_count_ =
          base::ClampAdd(unsent_samples_count_, samples_count.value());
    }
    list_value_->Append(std::move(dict_value));
    unsent_persisted_size_ += log->compressed_log_data.length();
  }

  base::HistogramBase::Count32 unsent_samples_count() const {
    return unsent_samples_count_;
  }

  size_t unsent_persisted_size() const { return unsent_persisted_size_; }

 private:
  // The list where the logs will be written to. This should represent a pref.
  raw_ptr<base::ListValue> list_value_;

  // The total number of histogram samples written so far.
  base::HistogramBase::Count32 unsent_samples_count_ = 0;

  // The total size of logs written so far.
  size_t unsent_persisted_size_ = 0;
};

bool GetString(const base::DictValue& dict,
               std::string_view key,
               std::string& out) {
  const std::string* value = dict.FindString(key);
  if (!value)
    return false;
  out = *value;
  return true;
}

}  // namespace

UnsentLogStore::LogInfo::LogInfo() = default;

UnsentLogStore::LogInfo::LogInfo(const std::string& log_data,
                                 const std::string& log_timestamp,
                                 const std::string& signing_key,
                                 const LogMetadata& optional_log_metadata) {
  DCHECK(!log_data.empty());

  if (!compression::GzipCompress(log_data, &compressed_log_data)) {
    DUMP_WILL_BE_NOTREACHED();
    return;
  }

  hash = Sha1ForUnsentLogStore(log_data);
  signature = ComputeHMACForLog(log_data, signing_key);

  timestamp = log_timestamp;
  log_metadata = optional_log_metadata;
}

UnsentLogStore::LogInfo::LogInfo(const std::string& log_data,
                                 const std::string& signing_key,
                                 const LogMetadata& optional_log_metadata)
    : LogInfo(log_data,
              base::NumberToString(base::Time::Now().ToTimeT()),
              signing_key,
              optional_log_metadata) {}

UnsentLogStore::LogInfo::~LogInfo() = default;

UnsentLogStore::UnsentLogStore(std::unique_ptr<UnsentLogStoreMetrics> metrics,
                               PrefService* local_state,
                               const char* log_data_pref_name,
                               const char* metadata_pref_name,
                               UnsentLogStoreLimits log_store_limits,
                               const std::string& signing_key,
                               MetricsLogsEventManager* logs_event_manager)
    : metrics_(std::move(metrics)),
      local_state_(local_state),
      log_data_pref_name_(log_data_pref_name),
      metadata_pref_name_(metadata_pref_name),
      log_store_limits_(log_store_limits),
      signing_key_(signing_key),
      logs_event_manager_(logs_event_manager) {
  DCHECK(local_state_);
  // One of the limit arguments must be non-zero.
  DCHECK(log_store_limits_.min_log_count > 0 ||
         log_store_limits_.min_queue_size_bytes > 0);
}

UnsentLogStore::~UnsentLogStore() = default;

bool UnsentLogStore::has_unsent_logs() const {
  return !!size();
}

// True if a log has been staged.
bool UnsentLogStore::has_staged_log() const {
  return staged_log_index_.has_value();
}

// Returns the compressed data of the element in the front of the list.
const std::string& UnsentLogStore::staged_log() const {
  return current_log()->compressed_log_data;
}

// Returns the hash of element in the front of the list.
const std::string& UnsentLogStore::staged_log_hash() const {
  return current_log()->hash;
}

// Returns the signature of element in the front of the list.
const std::string& UnsentLogStore::staged_log_signature() const {
  return current_log()->signature;
}

// Returns the timestamp of the element in the front of the list.
const std::string& UnsentLogStore::staged_log_timestamp() const {
  return current_log()->timestamp;
}

// Returns the user id of the current staged log.
std::optional<uint64_t> UnsentLogStore::staged_log_user_id() const {
  return current_log()->log_metadata.user_id;
}

const LogMetadata& UnsentLogStore::staged_log_metadata() const {
  return current_log()->log_metadata;
}

// static
std::string UnsentLogStore::ComputeHMACForLog(std::string_view log_data,
                                              std::string_view signing_key) {
  auto data = base::as_byte_span(log_data);
  auto key = base::as_byte_span(signing_key);
  std::array<uint8_t, crypto::hash::kSha256Size> hmac =
      crypto::hmac::SignSha256(key, data);
  return std::string(base::as_string_view(hmac));
}

void UnsentLogStore::StageNextLog() {
  // CHECK, rather than DCHECK, because swap()ing with an empty list causes
  // hard-to-identify crashes much later.
  CHECK(!list_.empty());
  DCHECK(!has_staged_log());
  staged_log_index_ = list_.size() - 1;
  NotifyLogEvent(MetricsLogsEventManager::LogEvent::kLogStaged,
                 current_log()->hash);
  DCHECK(has_staged_log());
}

void UnsentLogStore::DiscardStagedLogImpl(
    std::string_view reason,
    std::optional<UnsentLogDropReason> drop_reason) {
  DCHECK(has_staged_log());
  DCHECK_LT(staged_log_index_.value(), list_.size());
  if (drop_reason.has_value()) {
    metrics_->RecordDroppedLog(drop_reason.value());
  }
  NotifyLogEvent(MetricsLogsEventManager::LogEvent::kLogDiscarded,
                 current_log()->hash, reason);
  list_.erase(list_.begin() + staged_log_index_.value());
  staged_log_index_ = std::nullopt;
}

void UnsentLogStore::MarkStagedLogAsSent() {
  DCHECK(has_staged_log());
  DCHECK_LT(staged_log_index_.value(), list_.size());
  auto samples_count = current_log()->log_metadata.samples_count;
  if (samples_count.has_value()) {
    total_samples_sent_ =
        base::ClampAdd(total_samples_sent_, samples_count.value());
  }
  NotifyLogEvent(MetricsLogsEventManager::LogEvent::kLogUploaded,
                 current_log()->hash);
}

void UnsentLogStore::TrimAndPersistUnsentLogs(bool overwrite_in_memory_store) {
  const std::vector<std::optional<UnsentLogDropReason>> drop_reasons =
      GetTrimDropReasons();

  ScopedListPrefUpdate update(local_state_, log_data_pref_name_);
  LogsPrefWriter writer(&update.Get());

  for (size_t i = 0; i < list_.size(); ++i) {
    if (drop_reasons[i].has_value()) {
      if (drop_reasons[i] == UnsentLogDropReason::kTrimmedLogTooLarge) {
        // Record each oversized log's size before `RemoveTrimmedLogs()` can
        // remove the log from `list_`. Kept outside `RemoveTrimmedLogs()` so
        // disk-only trims (`overwrite_in_memory_store` is false) still emit it.
        metrics_->RecordDroppedLogSize(list_[i]->compressed_log_data.length());
      }
      continue;
    }

    // Append log to prefs.
    writer.WriteLogEntry(list_[i].get());
  }

  if (overwrite_in_memory_store) {
    RemoveTrimmedLogs(drop_reasons);
  }
  metrics_->RecordTrimmedLogs(drop_reasons, overwrite_in_memory_store);

  WriteToMetricsPref(writer.unsent_samples_count(), total_samples_sent_,
                     writer.unsent_persisted_size());
}

void UnsentLogStore::LoadPersistedUnsentLogs() {
  ReadLogsFromPrefList(local_state_->GetList(log_data_pref_name_));
  RecordMetaDataMetrics();
}

void UnsentLogStore::StoreLog(const std::string& log_data,
                              const LogMetadata& log_metadata,
                              MetricsLogsEventManager::CreateReason reason) {
  std::unique_ptr<LogInfo> info =
      std::make_unique<LogInfo>(log_data, signing_key_, log_metadata);
  StoreLogInfo(std::move(info), log_data.size(), reason);
}

void UnsentLogStore::StoreLogInfo(
    std::unique_ptr<LogInfo> log_info,
    size_t uncompressed_log_size,
    MetricsLogsEventManager::CreateReason reason) {
  DCHECK(log_info);
  metrics_->RecordCompressionRatio(log_info->compressed_log_data.size(),
                                   uncompressed_log_size);
  NotifyLogCreated(*log_info, reason);
  list_.emplace_back(std::move(log_info));
}

const std::string& UnsentLogStore::GetLogAtIndex(size_t index) {
  DCHECK_GE(index, 0U);
  DCHECK_LT(index, list_.size());
  return list_[index]->compressed_log_data;
}

std::string UnsentLogStore::ReplaceLogAtIndex(size_t index,
                                              const std::string& new_log_data,
                                              const LogMetadata& log_metadata) {
  DCHECK_GE(index, 0U);
  DCHECK_LT(index, list_.size());

  // Avoid copying of long strings.
  std::string old_log_data;
  old_log_data.swap(list_[index]->compressed_log_data);
  std::string old_timestamp;
  old_timestamp.swap(list_[index]->timestamp);
  std::string old_hash;
  old_hash.swap(list_[index]->hash);

  std::unique_ptr<LogInfo> info = std::make_unique<LogInfo>(
      new_log_data, old_timestamp, signing_key_, log_metadata);
  // Note that both the compression ratio of the new log and the log that is
  // being replaced are recorded.
  metrics_->RecordCompressionRatio(info->compressed_log_data.size(),
                                   new_log_data.size());

  // TODO(crbug.com/40238818): Pass a message to make it clear that the new log
  // is replacing the old log.
  NotifyLogEvent(MetricsLogsEventManager::LogEvent::kLogDiscarded, old_hash);
  NotifyLogCreated(*info, MetricsLogsEventManager::CreateReason::kUnknown);
  list_[index] = std::move(info);
  return old_log_data;
}

void UnsentLogStore::Purge() {
  metrics_->RecordDroppedLogs(UnsentLogDropReason::kPurged, list_.size());
  NotifyLogsEvent(list_, MetricsLogsEventManager::LogEvent::kLogDiscarded,
                  "Purged.");

  if (has_staged_log()) {
    // Do not call DropUnsentStagedLog(). The call above already recorded
    // this log.
    DiscardStagedLog();
  }
  list_.clear();
  local_state_->ClearPref(log_data_pref_name_);
  // The |total_samples_sent_| isn't cleared intentionally because it is still
  // meaningful.
  if (metadata_pref_name_)
    local_state_->ClearPref(metadata_pref_name_);
}

void UnsentLogStore::SetLogsEventManager(
    MetricsLogsEventManager* logs_event_manager) {
  logs_event_manager_ = logs_event_manager;
}

void UnsentLogStore::ReadLogsFromPrefList(const base::ListValue& list_value) {
  // The below DCHECK ensures that a log from prefs is not loaded multiple
  // times, which is important for the semantics of the NotifyLogsCreated() call
  // below.
  DCHECK(list_.empty());

  if (list_value.empty()) {
    metrics_->RecordLogReadStatus(UnsentLogStoreMetrics::LIST_EMPTY);
    return;
  }

  const size_t log_count = list_value.size();

  list_.resize(log_count);

  for (size_t i = 0; i < log_count; ++i) {
    const base::DictValue* dict = list_value[i].GetIfDict();
    std::unique_ptr<LogInfo> info = std::make_unique<LogInfo>();
    if (!dict || !GetString(*dict, kLogDataKey, info->compressed_log_data) ||
        !GetString(*dict, kLogHashKey, info->hash) ||
        !GetString(*dict, kLogTimestampKey, info->timestamp) ||
        !GetString(*dict, kLogSignatureKey, info->signature)) {
      // Something is wrong, so we don't try to get any persisted logs.
      list_.clear();
      metrics_->RecordCorruptLogRead(log_count);
      return;
    }

    info->compressed_log_data = DecodeFromBase64(info->compressed_log_data);
    info->hash = DecodeFromBase64(info->hash);
    info->signature = DecodeFromBase64(info->signature);
    // timestamp doesn't need to be decoded.

    // Extract the samples count of the log if it exists and is non-negative.
    // Absent when the log had no sample count or was saved by an older version.
    std::optional<int> samples_count = dict->FindInt(kLogSamplesCountKey);
    if (samples_count.has_value() && samples_count.value() >= 0) {
      info->log_metadata.samples_count = samples_count.value();
    }

    std::optional<int> log_source_type = dict->FindInt(kLogSourceType);
    if (log_source_type.has_value()) {
      info->log_metadata.log_source_type =
          static_cast<UkmLogSourceType>(log_source_type.value());
    }

    // Extract user id of the log if it exists.
    const std::string* user_id_str = dict->FindString(kLogUserIdKey);
    if (user_id_str) {
      uint64_t user_id;

      // Only initialize the metadata if conversion was successful.
      if (base::StringToUint64(DecodeFromBase64(*user_id_str), &user_id))
        info->log_metadata.user_id = user_id;
    }

    list_[i] = std::move(info);
  }

  // Only notify log observers after loading all logs from pref instead of
  // notifying as logs are loaded. This is because we may return early and end
  // up not loading any logs.
  NotifyLogsCreated(
      list_, MetricsLogsEventManager::CreateReason::kLoadFromPreviousSession);

  metrics_->RecordLogReadStatus(UnsentLogStoreMetrics::RECALL_SUCCESS);
}

void UnsentLogStore::WriteToMetricsPref(
    base::HistogramBase::Count32 unsent_samples_count,
    base::HistogramBase::Count32 sent_samples_count,
    size_t unsent_persisted_size) const {
  if (metadata_pref_name_ == nullptr) {
    return;
  }

  ScopedDictPrefUpdate update(local_state_, metadata_pref_name_);
  base::DictValue& pref_data = update.Get();
  pref_data.Set(kLogUnsentCountKey, unsent_samples_count);
  pref_data.Set(kLogSentCountKey, sent_samples_count);
  // Round up to kb.
  pref_data.Set(kLogPersistedSizeInKbKey,
                static_cast<int>(std::ceil(unsent_persisted_size / 1024.0)));
}

void UnsentLogStore::RecordMetaDataMetrics() {
  if (metadata_pref_name_ == nullptr) {
    return;
  }

  const base::DictValue& value = local_state_->GetDict(metadata_pref_name_);

  auto unsent_samples_count = value.FindInt(kLogUnsentCountKey);
  auto sent_samples_count = value.FindInt(kLogSentCountKey);
  auto unsent_persisted_size_in_kb = value.FindInt(kLogPersistedSizeInKbKey);

  if (unsent_samples_count && sent_samples_count &&
      unsent_persisted_size_in_kb) {
    metrics_->RecordLastUnsentLogMetadataMetrics(
        unsent_samples_count.value(), sent_samples_count.value(),
        unsent_persisted_size_in_kb.value());
  }
}

std::vector<std::optional<UnsentLogDropReason>>
UnsentLogStore::GetTrimDropReasons() const {
  std::vector<std::optional<UnsentLogDropReason>> drop_reasons(list_.size());
  if (!base::FeatureList::IsEnabled(features::kMetricsLogTrimming)) {
    return drop_reasons;
  }

  size_t bytes_used = 0;
  size_t kept_logs_count = 0;

  // Reverse order, so newest ones are prioritized.
  for (size_t i = list_.size(); i > 0; --i) {
    // Both minimums are met; drop the remaining older logs (including the
    // current one).
    if (bytes_used >= log_store_limits_.min_queue_size_bytes &&
        kept_logs_count >= log_store_limits_.min_log_count) {
      std::ranges::fill(base::span(drop_reasons).first(i),
                        UnsentLogDropReason::kTrimmedQueueLimit);
      break;
    }

    const size_t log_size = list_[i - 1]->compressed_log_data.length();
    // Omit overly large individual logs if the value is non-zero.
    if (log_store_limits_.max_log_size_bytes != 0 &&
        log_size > log_store_limits_.max_log_size_bytes) {
      drop_reasons[i - 1] = UnsentLogDropReason::kTrimmedLogTooLarge;
      continue;
    }

    bytes_used += log_size;
    ++kept_logs_count;
  }
  return drop_reasons;
}

void UnsentLogStore::RemoveTrimmedLogs(
    const std::vector<std::optional<UnsentLogDropReason>>& drop_reasons) {
  CHECK_EQ(drop_reasons.size(), list_.size());
  std::vector<std::unique_ptr<LogInfo>> kept_logs;
  // The new index of the staged log. Stays `std::nullopt` if there is no staged
  // log, or if the trim removes the staged log.
  std::optional<size_t> new_staged_log_index;

  for (size_t i = 0; i < list_.size(); ++i) {
    const std::optional<UnsentLogDropReason>& drop_reason = drop_reasons[i];
    if (!drop_reason.has_value()) {
      if (staged_log_index_ == i) {
        new_staged_log_index = kept_logs.size();
      }
      kept_logs.push_back(std::move(list_[i]));
      continue;
    }

    NotifyLogEvent(MetricsLogsEventManager::LogEvent::kLogTrimmed,
                   list_[i]->hash,
                   drop_reason == UnsentLogDropReason::kTrimmedLogTooLarge
                       ? "Log size too large."
                       : "");
  }

  // Put the kept logs in the correct place.
  list_.swap(kept_logs);

  // We may need to adjust the staged index since the number of logs may be
  // reduced.
  staged_log_index_ = new_staged_log_index;
}

void UnsentLogStore::NotifyLogCreated(
    const LogInfo& info,
    MetricsLogsEventManager::CreateReason reason) {
  if (!logs_event_manager_)
    return;
  logs_event_manager_->NotifyLogCreated(info.hash, info.compressed_log_data,
                                        info.timestamp, reason);
}

void UnsentLogStore::NotifyLogsCreated(
    base::span<std::unique_ptr<LogInfo>> logs,
    MetricsLogsEventManager::CreateReason reason) {
  if (!logs_event_manager_)
    return;
  for (const std::unique_ptr<LogInfo>& info : logs) {
    logs_event_manager_->NotifyLogCreated(info->hash, info->compressed_log_data,
                                          info->timestamp, reason);
  }
}

void UnsentLogStore::NotifyLogEvent(MetricsLogsEventManager::LogEvent event,
                                    std::string_view log_hash,
                                    std::string_view message) {
  if (!logs_event_manager_)
    return;
  logs_event_manager_->NotifyLogEvent(event, log_hash, message);
}

void UnsentLogStore::NotifyLogsEvent(base::span<std::unique_ptr<LogInfo>> logs,
                                     MetricsLogsEventManager::LogEvent event,
                                     std::string_view message) {
  if (!logs_event_manager_)
    return;
  for (const std::unique_ptr<LogInfo>& info : logs) {
    logs_event_manager_->NotifyLogEvent(event, info->hash, message);
  }
}

// Computes a SHA-1 hash of |data| and returns it as a string. This is
// required for backward compatibility with existing on-disk data. This function
// is intentionally declared in a separate header file "crypto/obsolete/sha1.h",
// so as to easily monitor current usage of SHA-1 in Chrome, since SHA-1 is now
// discouraged for new code.
std::string Sha1ForUnsentLogStore(std::string_view data) {
  return std::string(base::as_string_view(
      crypto::obsolete::Sha1::Hash(base::as_byte_span(data))));
}

const UnsentLogStore::LogInfo* UnsentLogStore::current_log() const {
  DCHECK(has_staged_log());
  return list_[staged_log_index_.value()].get();
}

UnsentLogStore::LogInfo* UnsentLogStore::current_log() {
  DCHECK(has_staged_log());
  return list_[staged_log_index_.value()].get();
}

}  // namespace metrics
