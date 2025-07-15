// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/web_ui/safe_browsing_ui_util.h"

#include "base/base64.h"
#include "base/base64url.h"
#include "base/i18n/number_formatting.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_writer.h"
#include "base/strings/utf_string_conversions.h"
#include "components/safe_browsing/core/common/proto/csd.to_value.h"

namespace safe_browsing::web_ui {

#if BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION) && !BUILDFLAG(IS_ANDROID)
DeepScanDebugData::DeepScanDebugData() = default;
DeepScanDebugData::DeepScanDebugData(const DeepScanDebugData&) = default;
DeepScanDebugData::~DeepScanDebugData() = default;

TailoredVerdictOverrideData::TailoredVerdictOverrideData() = default;
TailoredVerdictOverrideData::~TailoredVerdictOverrideData() = default;

void TailoredVerdictOverrideData::Set(
    ClientDownloadResponse::TailoredVerdict new_value,
    const SafeBrowsingUIHandler* new_source) {
  override_value = std::move(new_value);
  source = reinterpret_cast<SourceId>(new_source);
}

bool TailoredVerdictOverrideData::IsFromSource(
    const SafeBrowsingUIHandler* maybe_source) const {
  return reinterpret_cast<SourceId>(maybe_source) == source;
}

void TailoredVerdictOverrideData::Clear() {
  override_value.reset();
  source = 0u;
}
#endif

#if BUILDFLAG(SAFE_BROWSING_DB_LOCAL)

std::string UserReadableTimeFromMillisSinceEpoch(int64_t time_in_milliseconds) {
  base::Time time =
      base::Time::UnixEpoch() + base::Milliseconds(time_in_milliseconds);
  return base::UTF16ToUTF8(base::TimeFormatShortDateAndTime(time));
}

void AddStoreInfo(
    const DatabaseManagerInfo::DatabaseInfo::StoreInfo& store_info,
    base::Value::List& database_info_list) {
  if (store_info.has_file_name()) {
    database_info_list.Append(store_info.file_name());
  } else {
    database_info_list.Append("Unknown store");
  }

  base::Value::List store_info_list;
  if (store_info.has_file_size_bytes()) {
    store_info_list.Append(
        "Size (in bytes): " +
        base::UTF16ToUTF8(base::FormatNumber(store_info.file_size_bytes())));
  }

  if (store_info.has_update_status()) {
    store_info_list.Append(
        "Update status: " +
        base::UTF16ToUTF8(base::FormatNumber(store_info.update_status())));
  }

  if (store_info.has_last_apply_update_time_millis()) {
    store_info_list.Append("Last update time: " +
                           UserReadableTimeFromMillisSinceEpoch(
                               store_info.last_apply_update_time_millis()));
  }

  if (store_info.has_checks_attempted()) {
    store_info_list.Append(
        "Number of database checks: " +
        base::UTF16ToUTF8(base::FormatNumber(store_info.checks_attempted())));
  }

  if (store_info.has_state()) {
    std::string state_base64 = base::Base64Encode(store_info.state());
    store_info_list.Append("State: " + state_base64);
  }

  for (const auto& prefix_set : store_info.prefix_sets()) {
    std::string size = base::UTF16ToUTF8(base::FormatNumber(prefix_set.size()));
    std::string count =
        base::UTF16ToUTF8(base::FormatNumber(prefix_set.count()));
    store_info_list.Append(count + " prefixes of size " + size);
  }

  database_info_list.Append(std::move(store_info_list));
}

void AddDatabaseInfo(const DatabaseManagerInfo::DatabaseInfo& database_info,
                     base::Value::List& database_info_list) {
  if (database_info.has_database_size_bytes()) {
    database_info_list.Append("Database size (in bytes)");
    database_info_list.Append(
        static_cast<double>(database_info.database_size_bytes()));
  }

  // Add the information specific to each store.
  for (int i = 0; i < database_info.store_info_size(); i++) {
    AddStoreInfo(database_info.store_info(i), database_info_list);
  }
}

void AddUpdateInfo(const DatabaseManagerInfo::UpdateInfo& update_info,
                   base::Value::List& database_info_list) {
  if (update_info.has_network_status_code()) {
    // Network status of the last GetUpdate().
    database_info_list.Append("Last update network status code");
    database_info_list.Append(update_info.network_status_code());
  }
  if (update_info.has_last_update_time_millis()) {
    database_info_list.Append("Last update time");
    database_info_list.Append(UserReadableTimeFromMillisSinceEpoch(
        update_info.last_update_time_millis()));
  }
  if (update_info.has_next_update_time_millis()) {
    database_info_list.Append("Next update time");
    database_info_list.Append(UserReadableTimeFromMillisSinceEpoch(
        update_info.next_update_time_millis()));
  }
}

void ParseFullHashInfo(
    const FullHashCacheInfo::FullHashCache::CachedHashPrefixInfo::FullHashInfo&
        full_hash_info,
    base::Value::Dict& full_hash_info_dict) {
  if (full_hash_info.has_positive_expiry()) {
    full_hash_info_dict.Set(
        "Positive expiry",
        UserReadableTimeFromMillisSinceEpoch(full_hash_info.positive_expiry()));
  }
  if (full_hash_info.has_full_hash()) {
    std::string full_hash;
    base::Base64UrlEncode(full_hash_info.full_hash(),
                          base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                          &full_hash);
    full_hash_info_dict.Set("Full hash (base64)", std::move(full_hash));
  }
  if (full_hash_info.list_identifier().has_platform_type()) {
    full_hash_info_dict.Set("platform_type",
                            full_hash_info.list_identifier().platform_type());
  }
  if (full_hash_info.list_identifier().has_threat_entry_type()) {
    full_hash_info_dict.Set(
        "threat_entry_type",
        full_hash_info.list_identifier().threat_entry_type());
  }
  if (full_hash_info.list_identifier().has_threat_type()) {
    full_hash_info_dict.Set("threat_type",
                            full_hash_info.list_identifier().threat_type());
  }
}

void ParseFullHashCache(const FullHashCacheInfo::FullHashCache& full_hash_cache,
                        base::Value::List& full_hash_cache_list) {
  base::Value::Dict full_hash_cache_parsed;

  if (full_hash_cache.has_hash_prefix()) {
    std::string hash_prefix;
    base::Base64UrlEncode(full_hash_cache.hash_prefix(),
                          base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                          &hash_prefix);
    full_hash_cache_parsed.Set("Hash prefix (base64)", std::move(hash_prefix));
  }
  if (full_hash_cache.cached_hash_prefix_info().has_negative_expiry()) {
    full_hash_cache_parsed.Set(
        "Negative expiry",
        UserReadableTimeFromMillisSinceEpoch(
            full_hash_cache.cached_hash_prefix_info().negative_expiry()));
  }

  full_hash_cache_list.Append(std::move(full_hash_cache_parsed));

  for (const auto& full_hash_info_it :
       full_hash_cache.cached_hash_prefix_info().full_hash_info()) {
    base::Value::Dict full_hash_info_dict;
    ParseFullHashInfo(full_hash_info_it, full_hash_info_dict);
    full_hash_cache_list.Append(std::move(full_hash_info_dict));
  }
}

void ParseFullHashCacheInfo(const FullHashCacheInfo& full_hash_cache_info_proto,
                            base::Value::List& full_hash_cache_info) {
  if (full_hash_cache_info_proto.has_number_of_hits()) {
    base::Value::Dict number_of_hits;
    number_of_hits.Set("Number of cache hits",
                       full_hash_cache_info_proto.number_of_hits());
    full_hash_cache_info.Append(std::move(number_of_hits));
  }

  // Record FullHashCache list.
  for (const auto& full_hash_cache_it :
       full_hash_cache_info_proto.full_hash_cache()) {
    base::Value::List full_hash_cache_list;
    ParseFullHashCache(full_hash_cache_it, full_hash_cache_list);
    full_hash_cache_info.Append(std::move(full_hash_cache_list));
  }
}

std::string AddFullHashCacheInfo(
    const FullHashCacheInfo& full_hash_cache_info_proto) {
  base::Value::List full_hash_cache;
  ParseFullHashCacheInfo(full_hash_cache_info_proto, full_hash_cache);
  return SerializeJson(full_hash_cache);
}

#endif

std::string SerializeClientDownloadRequest(const ClientDownloadRequest& cdr) {
  return SerializeJson(Serialize(cdr));
}

std::string SerializeClientDownloadResponse(const ClientDownloadResponse& cdr) {
  return SerializeJson(Serialize(cdr));
}

std::string SerializeClientPhishingRequest(
    const ClientPhishingRequestAndToken& cprat) {
  base::Value::Dict value = Serialize(cprat.request);
  value.Set("scoped_oauthtoken", cprat.token);
  return SerializeJson(std::move(value));
}

std::string SerializeClientPhishingResponse(const ClientPhishingResponse& cpr) {
  return SerializeJson(Serialize(cpr));
}

std::string SerializeJson(base::ValueView value) {
  return base::WriteJsonWithOptions(value,
                                    base::JSONWriter::OPTIONS_PRETTY_PRINT)
      .value_or(std::string());
}

}  // namespace safe_browsing::web_ui
