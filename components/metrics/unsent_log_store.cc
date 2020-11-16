// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/unsent_log_store.h"

#include <cmath>
#include <memory>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/hash/sha1.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/timer/elapsed_timer.h"
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

std::string EncodeToBase64(const std::string& to_convert) {
  DCHECK(to_convert.data());
  std::string base64_result;
  base::Base64Encode(to_convert, &base64_result);
  return base64_result;
}

std::string DecodeFromBase64(const std::string& to_convert) {
  std::string result;
  base::Base64Decode(to_convert, &result);
  return result;
}

}  // namespace

UnsentLogStore::LogInfo::LogInfo() = default;
UnsentLogStore::LogInfo::LogInfo(const UnsentLogStore::LogInfo& other) =
    default;
UnsentLogStore::LogInfo::~LogInfo() = default;

void UnsentLogStore::LogInfo::Init(
    UnsentLogStoreMetrics* metrics,
    const std::string& log_data,
    const std::string& log_timestamp,
    const std::string& signing_key,
    base::Optional<base::HistogramBase::Count> samples_count) {
  DCHECK(!log_data.empty());

  if (!compression::GzipCompress(log_data, &compressed_log_data)) {
    NOTREACHED();
    return;
  }

  metrics->RecordCompressionRatio(compressed_log_data.size(), log_data.size());

  hash = base::SHA1HashString(log_data);

  // TODO(crbug.com/906202): Add an actual key for signing.
  crypto::HMAC hmac(crypto::HMAC::SHA256);
  const size_t digest_length = hmac.DigestLength();
  unsigned char* hmac_data = reinterpret_cast<unsigned char*>(
      base::WriteInto(&signature, digest_length + 1));
  if (!hmac.Init(signing_key) ||
      !hmac.Sign(log_data, hmac_data, digest_length)) {
    NOTREACHED() << "HMAC signing failed";
  }

  timestamp = log_timestamp;
  this->samples_count = samples_count;
}

UnsentLogStore::UnsentLogStore(std::unique_ptr<UnsentLogStoreMetrics> metrics,
                               PrefService* local_state,
                               const char* log_data_pref_name,
                               const char* metadata_pref_name,
                               size_t min_log_count,
                               size_t min_log_bytes,
                               size_t max_log_size,
                               const std::string& signing_key)
    : metrics_(std::move(metrics)),
      local_state_(local_state),
      log_data_pref_name_(log_data_pref_name),
      metadata_pref_name_(metadata_pref_name),
      min_log_count_(min_log_count),
      min_log_bytes_(min_log_bytes),
      max_log_size_(max_log_size != 0 ? max_log_size : static_cast<size_t>(-1)),
      signing_key_(signing_key),
      staged_log_index_(-1) {
  DCHECK(local_state_);
  // One of the limit arguments must be non-zero.
  DCHECK(min_log_count_ > 0 || min_log_bytes_ > 0);
}

UnsentLogStore::~UnsentLogStore() {}

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

void UnsentLogStore::StageNextLog() {
  // CHECK, rather than DCHECK, because swap()ing with an empty list causes
  // hard-to-identify crashes much later.
  CHECK(!list_.empty());
  DCHECK(!has_staged_log());
  staged_log_index_ = list_.size() - 1;
  DCHECK(has_staged_log());
}

void UnsentLogStore::DiscardStagedLog() {
  DCHECK(has_staged_log());
  DCHECK_LT(static_cast<size_t>(staged_log_index_), list_.size());
  list_.erase(list_.begin() + staged_log_index_);
  staged_log_index_ = -1;
}

void UnsentLogStore::MarkStagedLogAsSent() {
  DCHECK(has_staged_log());
  DCHECK_LT(static_cast<size_t>(staged_log_index_), list_.size());
  if (list_[staged_log_index_]->samples_count.has_value())
    total_samples_sent_ += list_[staged_log_index_]->samples_count.value();
}

void UnsentLogStore::TrimAndPersistUnsentLogs() {
  ListPrefUpdate update(local_state_, log_data_pref_name_);
  // TODO(crbug.com/859477): Verify that the preference has been properly
  // registered.
  CHECK(update.Get());
  TrimLogs();
  WriteLogsToPrefList(update.Get());
}

void UnsentLogStore::LoadPersistedUnsentLogs() {
  ReadLogsFromPrefList(*local_state_->GetList(log_data_pref_name_));
  RecordMetaDataMertics();
}

void UnsentLogStore::StoreLog(
    const std::string& log_data,
    base::Optional<base::HistogramBase::Count> samples_count) {
  LogInfo info;
  info.Init(metrics_.get(), log_data,
            base::NumberToString(base::Time::Now().ToTimeT()), signing_key_,
            samples_count);
  list_.emplace_back(std::make_unique<LogInfo>(info));
}

const std::string& UnsentLogStore::GetLogAtIndex(size_t index) {
  DCHECK_GE(index, 0U);
  DCHECK_LT(index, list_.size());
  return list_[index]->compressed_log_data;
}

std::string UnsentLogStore::ReplaceLogAtIndex(
    size_t index,
    const std::string& new_log_data,
    base::Optional<base::HistogramBase::Count> samples_count) {
  DCHECK_GE(index, 0U);
  DCHECK_LT(index, list_.size());

  // Avoid copying of long strings.
  std::string old_log_data;
  old_log_data.swap(list_[index]->compressed_log_data);
  std::string old_timestamp;
  old_timestamp.swap(list_[index]->timestamp);

  // TODO(rkaplow): Would be a bit simpler if we had a method that would
  // just return a pointer to the logInfo so we could combine the next 3 lines.
  LogInfo info;
  info.Init(metrics_.get(), new_log_data, old_timestamp, signing_key_,
            samples_count);

  list_[index] = std::make_unique<LogInfo>(info);
  return old_log_data;
}

void UnsentLogStore::Purge() {
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

void UnsentLogStore::ReadLogsFromPrefList(const base::ListValue& list_value) {
  if (list_value.empty()) {
    metrics_->RecordLogReadStatus(UnsentLogStoreMetrics::LIST_EMPTY);
    return;
  }

  const size_t log_count = list_value.GetSize();

  DCHECK(list_.empty());
  list_.resize(log_count);

  for (size_t i = 0; i < log_count; ++i) {
    const base::DictionaryValue* dict;
    LogInfo info;
    if (!list_value.GetDictionary(i, &dict) ||
        !dict->GetString(kLogDataKey, &info.compressed_log_data) ||
        !dict->GetString(kLogHashKey, &info.hash) ||
        !dict->GetString(kLogTimestampKey, &info.timestamp) ||
        !dict->GetString(kLogSignatureKey, &info.signature)) {
      // Something is wrong, so we don't try to get any persisted logs.
      list_.clear();
      metrics_->RecordLogReadStatus(
          UnsentLogStoreMetrics::LOG_STRING_CORRUPTION);
      return;
    }

    info.compressed_log_data = DecodeFromBase64(info.compressed_log_data);
    info.hash = DecodeFromBase64(info.hash);
    info.signature = DecodeFromBase64(info.signature);
    // timestamp doesn't need to be decoded.

    list_[i] = std::make_unique<LogInfo>(info);
  }

  metrics_->RecordLogReadStatus(UnsentLogStoreMetrics::RECALL_SUCCESS);
}

void UnsentLogStore::TrimLogs() {
  std::vector<std::unique_ptr<LogInfo>> trimmed_list;
  size_t bytes_used = 0;

  // The distance of the staged log from the end of the list of logs. Usually
  // this is 0 (end of list). We mark so we can correct adjust the
  // staged_log_index after log trimming.
  size_t staged_index_distance = 0;

  // Reverse order, so newest ones are prioritized.
  for (int i = list_.size() - 1; i >= 0; --i) {
    size_t log_size = list_[i]->compressed_log_data.length();
    // Hit the caps, we can stop moving the logs.
    if (bytes_used >= min_log_bytes_ && trimmed_list.size() >= min_log_count_) {
      break;
    }
    // Omit overly large individual logs.
    if (log_size > max_log_size_) {
      metrics_->RecordDroppedLogSize(log_size);
      continue;
    }

    bytes_used += log_size;

    if (staged_log_index_ == i) {
      staged_index_distance = trimmed_list.size();
    }

    trimmed_list.emplace_back(std::move(list_[i]));
  }

  // We went in reverse order, but appended entries. So reverse list to correct.
  std::reverse(trimmed_list.begin(), trimmed_list.end());

  size_t dropped_logs_count = list_.size() - trimmed_list.size();
  if (dropped_logs_count > 0)
    metrics_->RecordDroppedLogsNum(dropped_logs_count);

  // Put the trimmed list in the correct place.
  list_.swap(trimmed_list);

  // We may need to adjust the staged index since the number of logs may be
  // reduced. However, we want to make sure not to change the index if there is
  // no log staged.
  if (staged_log_index_ != -1) {
    staged_log_index_ = list_.size() - 1 - staged_index_distance;
  }
}

void UnsentLogStore::WriteLogsToPrefList(base::ListValue* list_value) const {
  list_value->Clear();

  base::HistogramBase::Count unsent_samples_count = 0;
  size_t unsent_persisted_size = 0;

  for (auto& log : list_) {
    std::unique_ptr<base::DictionaryValue> dict_value(
        new base::DictionaryValue);
    dict_value->SetString(kLogHashKey, EncodeToBase64(log->hash));
    dict_value->SetString(kLogSignatureKey, EncodeToBase64(log->signature));
    dict_value->SetString(kLogDataKey,
                          EncodeToBase64(log->compressed_log_data));
    dict_value->SetString(kLogTimestampKey, log->timestamp);
    list_value->Append(std::move(dict_value));

    if (log->samples_count.has_value()) {
      unsent_samples_count += log->samples_count.value();
    }
    unsent_persisted_size += log->compressed_log_data.length();
  }
  WriteToMetricsPref(unsent_samples_count, total_samples_sent_,
                     unsent_persisted_size);
}

void UnsentLogStore::WriteToMetricsPref(
    base::HistogramBase::Count unsent_samples_count,
    base::HistogramBase::Count sent_samples_count,
    size_t unsent_persisted_size) const {
  if (metadata_pref_name_ == nullptr)
    return;

  DictionaryPrefUpdate update(local_state_, metadata_pref_name_);
  base::DictionaryValue* pref_data = update.Get();
  pref_data->SetKey(kLogUnsentCountKey, base::Value(unsent_samples_count));
  pref_data->SetKey(kLogSentCountKey, base::Value(sent_samples_count));
  // Round up to kb.
  pref_data->SetKey(
      kLogPersistedSizeInKbKey,
      base::Value(static_cast<int>(std::ceil(unsent_persisted_size / 1024.0))));
}

void UnsentLogStore::RecordMetaDataMertics() {
  if (metadata_pref_name_ == nullptr)
    return;

  const base::DictionaryValue* value =
      local_state_->GetDictionary(metadata_pref_name_);
  if (!value)
    return;

  auto unsent_samples_count = value->FindIntKey(kLogUnsentCountKey);
  auto sent_samples_count = value->FindIntKey(kLogSentCountKey);
  auto unsent_persisted_size_in_kb =
      value->FindIntKey(kLogPersistedSizeInKbKey);

  if (unsent_samples_count && sent_samples_count &&
      unsent_persisted_size_in_kb) {
    metrics_->RecordLastUnsentLogMetadataMetrics(
        unsent_samples_count.value(), sent_samples_count.value(),
        unsent_persisted_size_in_kb.value());
  }
}

}  // namespace metrics
