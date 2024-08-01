// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/metrics/unsent_log_store.h"

#include <cmath>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/base64.h"
#include "base/hash/sha1.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/timer/elapsed_timer.h"
#include "components/metrics/metrics_features.h"
#include "components/metrics/unsent_log_store_metrics.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "crypto/hmac.h"
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
  // should be a base::Value::List representing a pref. Clears the contents of
  // |list_value|.
  explicit LogsPrefWriter(base::Value::List* list_value)
      : list_value_(list_value) {
    DCHECK(list_value);
    list_value->clear();
  }

  LogsPrefWriter(const LogsPrefWriter&) = delete;
  LogsPrefWriter& operator=(const LogsPrefWriter&) = delete;

  ~LogsPrefWriter() { DCHECK(finished_); }

  // Persists |log| by appending it to |list_value_|.
  void WriteLogEntry(UnsentLogStore::LogInfo* log) {
    DCHECK(!finished_);

    base::Value::Dict dict_value;
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
    list_value_->Append(std::move(dict_value));

    auto samples_count = log->log_metadata.samples_count;
    if (samples_count.has_value()) {
      unsent_samples_count_ += samples_count.value();
    }
    unsent_persisted_size_ += log->compressed_log_data.length();
    ++unsent_logs_count_;
  }

  // Indicates to this writer that it is finished, and that it should not write
  // any more logs. This also reverses |list_value_| in order to maintain the
  // original order of the logs that were written.
  void Finish() {
    DCHECK(!finished_);
    finished_ = true;
    std::reverse(list_value_->begin(), list_value_->end());
  }

  base::HistogramBase::Count unsent_samples_count() const {
    return unsent_samples_count_;
  }

  size_t unsent_persisted_size() const { return unsent_persisted_size_; }

  size_t unsent_logs_count() const { return unsent_logs_count_; }

 private:
  // The list where the logs will be written to. This should represent a pref.
  raw_ptr<base::Value::List> list_value_;

  // Whether or not this writer has finished writing to pref.
  bool finished_ = false;

  // The total number of histogram samples written so far.
  base::HistogramBase::Count unsent_samples_count_ = 0;

  // The total size of logs written so far.
  size_t unsent_persisted_size_ = 0;

  // The total number of logs written so far.
  size_t unsent_logs_count_ = 0;
};

bool GetString(const base::Value::Dict& dict,
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
UnsentLogStore::LogInfo::~LogInfo() = default;

void UnsentLogStore::LogInfo::Init(const std::string& log_data,
                                   const std::string& log_timestamp,
                                   const std::string& signing_key,
                                   const LogMetadata& optional_log_metadata) {
  DCHECK(!log_data.empty());

  if (!compression::GzipCompress(log_data, &compressed_log_data)) {
    DUMP_WILL_BE_NOTREACHED();
    return;
  }

  hash = base::SHA1HashString(log_data);

  if (!ComputeHMACForLog(log_data, signing_key, &signature)) {
    NOTREACHED_IN_MIGRATION() << "HMAC signing failed";
  }

  timestamp = log_timestamp;
  this->log_metadata = optional_log_metadata;
}

void UnsentLogStore::LogInfo::Init(const std::string& log_data,
                                   const std::string& signing_key,
                                   const LogMetadata& optional_log_metadata) {
  Init(log_data, base::NumberToString(base::Time::Now().ToTimeT()), signing_key,
       optional_log_metadata);
}

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
      logs_event_manager_(logs_event_manager),
      staged_log_index_(-1) {
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
  return staged_log_index_ != -1;
}

// Returns the compressed data of the element in the front of the list.
const std::string& UnsentLogStore::staged_log() const {
  DCHECK(has_staged_log());
  return list_[staged_log_index_]->compressed_log_data;
}

// Returns the hash of element in the front of the list.
const std::string& UnsentLogStore::staged_log_hash() const {
  DCHECK(has_staged_log());
  return list_[staged_log_index_]->hash;
}

// Returns the signature of element in the front of the list.
const std::string& UnsentLogStore::staged_log_signature() const {
  DCHECK(has_staged_log());
  return list_[staged_log_index_]->signature;
}

// Returns the timestamp of the element in the front of the list.
const std::string& UnsentLogStore::staged_log_timestamp() const {
  DCHECK(has_staged_log());
  return list_[staged_log_index_]->timestamp;
}

// Returns the user id of the current staged log.
std::optional<uint64_t> UnsentLogStore::staged_log_user_id() const {
  DCHECK(has_staged_log());
  return list_[staged_log_index_]->log_metadata.user_id;
}

const LogMetadata UnsentLogStore::staged_log_metadata() const {
  DCHECK(has_staged_log());
  return std::move(list_[staged_log_index_]->log_metadata);
}

// static
bool UnsentLogStore::ComputeHMACForLog(const std::string& log_data,
                                       const std::string& signing_key,
                                       std::string* signature) {
  crypto::HMAC hmac(crypto::HMAC::SHA256);
  const size_t digest_length = hmac.DigestLength();
  unsigned char* hmac_data = reinterpret_cast<unsigned char*>(
      base::WriteInto(signature, digest_length + 1));
  return hmac.Init(signing_key) &&
         hmac.Sign(log_data, hmac_data, digest_length);
}

void UnsentLogStore::StageNextLog() {
  // CHECK, rather than DCHECK, because swap()ing with an empty list causes
  // hard-to-identify crashes much later.
  CHECK(!list_.empty());
  DCHECK(!has_staged_log());
  staged_log_index_ = list_.size() - 1;
  NotifyLogEvent(MetricsLogsEventManager::LogEvent::kLogStaged,
                 list_[staged_log_index_]->hash);
  DCHECK(has_staged_log());
}

void UnsentLogStore::DiscardStagedLog(std::string_view reason) {
  DCHECK(has_staged_log());
  DCHECK_LT(static_cast<size_t>(staged_log_index_), list_.size());
  NotifyLogEvent(MetricsLogsEventManager::LogEvent::kLogDiscarded,
                 list_[staged_log_index_]->hash, reason);
  list_.erase(list_.begin() + staged_log_index_);
  staged_log_index_ = -1;
}

void UnsentLogStore::MarkStagedLogAsSent() {
  DCHECK(has_staged_log());
  DCHECK_LT(static_cast<size_t>(staged_log_index_), list_.size());
  auto samples_count = list_[staged_log_index_]->log_metadata.samples_count;
  if (samples_count.has_value())
    total_samples_sent_ += samples_count.value();
  NotifyLogEvent(MetricsLogsEventManager::LogEvent::kLogUploaded,
                 list_[staged_log_index_]->hash);
}

void UnsentLogStore::TrimAndPersistUnsentLogs(bool overwrite_in_memory_store) {
  ScopedListPrefUpdate update(local_state_, log_data_pref_name_);
  LogsPrefWriter writer(&update.Get());

  std::vector<std::unique_ptr<LogInfo>> trimmed_list;
  size_t bytes_used = 0;

  // The distance of the staged log from the end of the list of logs, which is
  // usually 0 (end of list). This is used in case there is currently a staged
  // log, which may or may not get trimmed. We want to keep track of the new
  // position of the staged log after trimming so that we can update
  // |staged_log_index_|.
  std::optional<size_t> staged_index_distance;

  const bool trimming_enabled =
      base::FeatureList::IsEnabled(features::kMetricsLogTrimming);

  // Reverse order, so newest ones are prioritized.
  for (int i = list_.size() - 1; i >= 0; --i) {
    size_t log_size = list_[i]->compressed_log_data.length();

    if (trimming_enabled) {
      // Hit the caps, we can stop moving the logs.
      if (bytes_used >= log_store_limits_.min_queue_size_bytes &&
          writer.unsent_logs_count() >= log_store_limits_.min_log_count) {
        // The rest of the logs (including the current one) are trimmed.
        if (overwrite_in_memory_store) {
          NotifyLogsEvent(base::span<std::unique_ptr<LogInfo>>(
                              list_.begin(), list_.begin() + i + 1),
                          MetricsLogsEventManager::LogEvent::kLogTrimmed);
        }
        break;
      }
      // Omit overly large individual logs if the value is non-zero.
      if (log_store_limits_.max_log_size_bytes != 0 &&
          log_size > log_store_limits_.max_log_size_bytes) {
        metrics_->RecordDroppedLogSize(log_size);
        if (overwrite_in_memory_store) {
          NotifyLogEvent(MetricsLogsEventManager::LogEvent::kLogTrimmed,
                         list_[i]->hash, "Log size too large.");
        }
        continue;
      }
    }

    bytes_used += log_size;

    if (staged_log_index_ == i) {
      staged_index_distance = writer.unsent_logs_count();
    }

    // Append log to prefs.
    writer.WriteLogEntry(list_[i].get());
    if (overwrite_in_memory_store) {
      trimmed_list.emplace_back(std::move(list_[i]));
    }
  }

  writer.Finish();

  if (overwrite_in_memory_store) {
    // We went in reverse order, but appended entries. So reverse list to
    // correct.
    std::reverse(trimmed_list.begin(), trimmed_list.end());

    size_t dropped_logs_count = list_.size() - trimmed_list.size();
    if (dropped_logs_count > 0)
      metrics_->RecordDroppedLogsNum(dropped_logs_count);

    // Put the trimmed list in the correct place.
    list_.swap(trimmed_list);

    // We may need to adjust the staged index since the number of logs may be
    // reduced.
    if (staged_index_distance.has_value()) {
      staged_log_index_ = list_.size() - 1 - staged_index_distance.value();
    } else {
      // Set |staged_log_index_| to -1. It might already be -1. E.g., at the
      // time we are trimming logs, there was no staged log. However, it is also
      // possible that we trimmed away the staged log, so we need to update the
      // index to -1.
      staged_log_index_ = -1;
    }
  }

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
  std::unique_ptr<LogInfo> info = std::make_unique<LogInfo>();
  info->Init(log_data, signing_key_, log_metadata);
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

  std::unique_ptr<LogInfo> info = std::make_unique<LogInfo>();
  info->Init(new_log_data, old_timestamp, signing_key_, log_metadata);
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
  NotifyLogsEvent(list_, MetricsLogsEventManager::LogEvent::kLogDiscarded,
                  "Purged.");

  if (has_staged_log()) {
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

void UnsentLogStore::ReadLogsFromPrefList(const base::Value::List& list_value) {
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
    const base::Value::Dict* dict = list_value[i].GetIfDict();
    std::unique_ptr<LogInfo> info = std::make_unique<LogInfo>();
    if (!dict || !GetString(*dict, kLogDataKey, info->compressed_log_data) ||
        !GetString(*dict, kLogHashKey, info->hash) ||
        !GetString(*dict, kLogTimestampKey, info->timestamp) ||
        !GetString(*dict, kLogSignatureKey, info->signature)) {
      // Something is wrong, so we don't try to get any persisted logs.
      list_.clear();
      metrics_->RecordLogReadStatus(
          UnsentLogStoreMetrics::LOG_STRING_CORRUPTION);
      return;
    }

    info->compressed_log_data = DecodeFromBase64(info->compressed_log_data);
    info->hash = DecodeFromBase64(info->hash);
    info->signature = DecodeFromBase64(info->signature);
    // timestamp doesn't need to be decoded.

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
    base::HistogramBase::Count unsent_samples_count,
    base::HistogramBase::Count sent_samples_count,
    size_t unsent_persisted_size) const {
  if (metadata_pref_name_ == nullptr)
    return;

  ScopedDictPrefUpdate update(local_state_, metadata_pref_name_);
  base::Value::Dict& pref_data = update.Get();
  pref_data.Set(kLogUnsentCountKey, unsent_samples_count);
  pref_data.Set(kLogSentCountKey, sent_samples_count);
  // Round up to kb.
  pref_data.Set(kLogPersistedSizeInKbKey,
                static_cast<int>(std::ceil(unsent_persisted_size / 1024.0)));
}

void UnsentLogStore::RecordMetaDataMetrics() {
  if (metadata_pref_name_ == nullptr)
    return;

  const base::Value::Dict& value = local_state_->GetDict(metadata_pref_name_);

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

}  // namespace metrics
