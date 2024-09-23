// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/feed/core/v2/types.h"

#include <ostream>
#include <string_view>
#include <utility>

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/json/values_util.h"
#include "base/pickle.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/feed/core/v2/proto_util.h"
#include "components/feed/core/v2/public/types.h"

// Note: This file contains implementation for both types.h and public/types.h.
//       because our build system will not allow multiple types.cc files in the
//       same target.

namespace feed {
namespace {

void PickleNetworkResponseInfo(const NetworkResponseInfo& value,
                               base::Pickle& pickle) {
  pickle.WriteInt(value.status_code);
  pickle.WriteUInt64(value.fetch_duration.InMilliseconds());
  pickle.WriteUInt64(
      (value.fetch_time - base::Time::UnixEpoch()).InMilliseconds());
  pickle.WriteString(value.bless_nonce);
  pickle.WriteString(value.base_request_url.spec());
}

bool UnpickleNetworkResponseInfo(base::PickleIterator& iterator,
                                 NetworkResponseInfo& value) {
  uint64_t fetch_duration_ms;
  uint64_t fetch_time_ms;
  std::string base_request_url;
  if (!(iterator.ReadInt(&value.status_code) &&
        iterator.ReadUInt64(&fetch_duration_ms) &&
        iterator.ReadUInt64(&fetch_time_ms) &&
        iterator.ReadString(&value.bless_nonce) &&
        iterator.ReadString(&base_request_url)))
    return false;
  value.fetch_duration = base::Milliseconds(fetch_duration_ms);
  value.fetch_time =
      base::Milliseconds(fetch_time_ms) + base::Time::UnixEpoch();
  value.base_request_url = GURL(base_request_url);
  return true;
}

void PickleOptionalNetworkResponseInfo(
    const std::optional<NetworkResponseInfo>& value,
    base::Pickle& pickle) {
  if (value.has_value()) {
    pickle.WriteBool(true);
    PickleNetworkResponseInfo(*value, pickle);
  } else {
    pickle.WriteBool(false);
  }
}

bool UnpickleOptionalNetworkResponseInfo(
    base::PickleIterator& iterator,
    std::optional<NetworkResponseInfo>& value) {
  bool has_network_response_info = false;
  if (!iterator.ReadBool(&has_network_response_info))
    return false;

  if (has_network_response_info) {
    NetworkResponseInfo reponse_info;
    if (!UnpickleNetworkResponseInfo(iterator, reponse_info))
      return false;
    value = std::move(reponse_info);
  } else {
    value.reset();
  }
  return true;
}

void PickleDebugStreamData(const DebugStreamData& value, base::Pickle& pickle) {
  pickle.WriteInt(DebugStreamData::kVersion);
  PickleOptionalNetworkResponseInfo(value.fetch_info, pickle);
  PickleOptionalNetworkResponseInfo(value.upload_info, pickle);
  pickle.WriteString(value.load_stream_status);
}

bool UnpickleDebugStreamData(base::PickleIterator iterator,
                             DebugStreamData& value) {
  int version;
  return iterator.ReadInt(&version) && version == DebugStreamData::kVersion &&
         UnpickleOptionalNetworkResponseInfo(iterator, value.fetch_info) &&
         UnpickleOptionalNetworkResponseInfo(iterator, value.upload_info) &&
         iterator.ReadString(&value.load_stream_status);
}

std::vector<uint32_t> GetExpandedHashes(
    const std::vector<feedstore::StreamContentHashList>& hashes_list) {
  std::vector<uint32_t> expanded_hashes;
  for (const feedstore::StreamContentHashList& hash_list : hashes_list)
    for (const auto& hash : hash_list.hashes())
      expanded_hashes.push_back(hash);
  return expanded_hashes;
}

}  // namespace

RequestMetadata::RequestMetadata() = default;
RequestMetadata::~RequestMetadata() = default;
RequestMetadata::RequestMetadata(RequestMetadata&&) = default;
RequestMetadata& RequestMetadata::operator=(RequestMetadata&&) = default;
feedwire::ClientInfo RequestMetadata::ToClientInfo() const {
  return CreateClientInfo(*this);
}

NetworkResponseInfo::NetworkResponseInfo() = default;
NetworkResponseInfo::NetworkResponseInfo(const NetworkResponseInfo&) = default;
NetworkResponseInfo::NetworkResponseInfo(NetworkResponseInfo&&) = default;
NetworkResponseInfo& NetworkResponseInfo::operator=(
    const NetworkResponseInfo&) = default;
NetworkResponseInfo& NetworkResponseInfo::operator=(NetworkResponseInfo&&) =
    default;
NetworkResponseInfo::~NetworkResponseInfo() = default;

NetworkResponse::NetworkResponse() = default;
NetworkResponse::NetworkResponse(const std::string& response_bytes,
                                 int status_code)
    : response_bytes(response_bytes), status_code(status_code) {}
NetworkResponse::~NetworkResponse() = default;
NetworkResponse::NetworkResponse(const NetworkResponse&) = default;
NetworkResponse& NetworkResponse::operator=(const NetworkResponse&) = default;

std::string ToString(ContentRevision c) {
  // The 'c/' prefix is used to identify slices as content. Don't change this
  // without updating the Java side.
  return base::StrCat({"c/", base::NumberToString(c.value())});
}

ContentRevision ToContentRevision(const std::string& str) {
  if (str.size() < 3)
    return {};

  uint32_t value;
  if (str[0] == 'c' && str[1] == '/' &&
      base::StringToUint(std::string_view(str).substr(2), &value)) {
    return ContentRevision(value);
  }
  return {};
}

std::string SerializeDebugStreamData(const DebugStreamData& data) {
  base::Pickle pickle;
  PickleDebugStreamData(data, pickle);
  const uint8_t* pickle_data_ptr = static_cast<const uint8_t*>(pickle.data());
  return base::Base64Encode(
      base::span<const uint8_t>(pickle_data_ptr, pickle.size()));
}

std::optional<DebugStreamData> DeserializeDebugStreamData(
    std::string_view base64_encoded) {
  std::string binary_data;
  if (!base::Base64Decode(base64_encoded, &binary_data))
    return std::nullopt;
  base::Pickle pickle =
      base::Pickle::WithUnownedBuffer(base::as_byte_span(binary_data));
  DebugStreamData result;
  if (!UnpickleDebugStreamData(base::PickleIterator(pickle), result))
    return std::nullopt;
  return result;
}

DebugStreamData::DebugStreamData() = default;
DebugStreamData::~DebugStreamData() = default;
DebugStreamData::DebugStreamData(const DebugStreamData&) = default;
DebugStreamData& DebugStreamData::operator=(const DebugStreamData&) = default;

base::Value::Dict PersistentMetricsDataToDict(
    const PersistentMetricsData& data) {
  base::Value::Dict dict;
  dict.Set("day_start", base::TimeToValue(data.current_day_start));
  dict.Set("time_spent_in_feed",
           base::TimeDeltaToValue(data.accumulated_time_spent_in_feed));
  dict.Set("visit_start", base::TimeToValue(data.visit_start));
  dict.Set("visit_end", base::TimeToValue(data.visit_end));
  dict.Set("did_report_good_visit", base::Value(data.did_report_good_visit));
  dict.Set("time_in_feed_for_good_visit",
           base::TimeDeltaToValue(data.time_in_feed_for_good_visit));
  dict.Set("did_scroll_in_visit", base::Value(data.did_scroll_in_visit));
  return dict;
}

PersistentMetricsData PersistentMetricsDataFromDict(
    const base::Value::Dict& dict) {
  PersistentMetricsData result;
  std::optional<base::Time> day_start =
      base::ValueToTime(dict.Find("day_start"));
  if (!day_start)
    return result;
  result.current_day_start = *day_start;
  std::optional<base::TimeDelta> time_spent_in_feed =
      base::ValueToTimeDelta(dict.Find("time_spent_in_feed"));
  if (time_spent_in_feed) {
    result.accumulated_time_spent_in_feed = *time_spent_in_feed;
  }

  result.visit_start =
      base::ValueToTime(dict.Find("visit_start")).value_or(base::Time());
  result.visit_end =
      base::ValueToTime(dict.Find("visit_end")).value_or(base::Time());

  if (const base::Value* value = dict.Find("did_report_good_visit"))
    result.did_report_good_visit = value->GetIfBool().value_or(false);

  result.time_in_feed_for_good_visit =
      base::ValueToTimeDelta(dict.Find("time_in_feed_for_good_visit"))
          .value_or(base::Seconds(0));

  if (const base::Value* value = dict.Find("did_scroll_in_visit"))
    result.did_scroll_in_visit = value->GetIfBool().value_or(false);

  return result;
}

LoadLatencyTimes::LoadLatencyTimes() : last_time_(base::TimeTicks::Now()) {}
LoadLatencyTimes::~LoadLatencyTimes() = default;
void LoadLatencyTimes::StepComplete(StepKind kind) {
  auto now = base::TimeTicks::Now();
  steps_.push_back(Step{kind, now - last_time_});
  last_time_ = now;
}

ContentHashSet::ContentHashSet() = default;
ContentHashSet::~ContentHashSet() = default;
ContentHashSet::ContentHashSet(
    std::vector<feedstore::StreamContentHashList> hashes)
    : original_hashes_(std::move(hashes)),
      sorted_hashes_(GetExpandedHashes(original_hashes_)) {}
ContentHashSet::ContentHashSet(const ContentHashSet&) = default;
ContentHashSet::ContentHashSet(ContentHashSet&&) = default;
ContentHashSet& ContentHashSet::operator=(const ContentHashSet&) = default;
ContentHashSet& ContentHashSet::operator=(ContentHashSet&&) = default;
bool ContentHashSet::ContainsAllOf(const ContentHashSet& items) const {
  for (uint32_t hash : items.sorted_hashes_) {
    if (!sorted_hashes_.contains(hash))
      return false;
  }
  return true;
}
bool ContentHashSet::Contains(uint32_t hash) const {
  return sorted_hashes_.contains(hash);
}
bool ContentHashSet::IsEmpty() const {
  return sorted_hashes_.empty();
}
bool ContentHashSet::operator==(const ContentHashSet& rhs) const {
  return sorted_hashes_ == rhs.sorted_hashes_;
}
std::ostream& operator<<(std::ostream& s, const ContentHashSet& hash_set) {
  s << "{";
  for (const auto& hash_list : hash_set.original_hashes()) {
    s << "(";
    for (const auto hash : hash_list.hashes()) {
      s << hash << ", ";
    }
    s << ")";
  }
  s << "} {";
  for (const auto& hash : hash_set.sorted_hashes()) {
    s << hash << ", ";
  }
  s << "}";
  return s;
}

LaunchResult::LaunchResult(LoadStreamStatus load_stream_status,
                           feedwire::DiscoverLaunchResult launch_result)
    : load_stream_status(load_stream_status), launch_result(launch_result) {}
LaunchResult::LaunchResult(const LaunchResult& other) = default;
LaunchResult::~LaunchResult() = default;
LaunchResult& LaunchResult::operator=(const LaunchResult& other) = default;

}  // namespace feed
