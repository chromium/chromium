// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/android/real_time_url_checks_allowlist.h"

#include "base/functional/bind.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "components/grit/components_resources.h"
#include "components/safe_browsing/android/proto/realtimeallowlist.pb.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace safe_browsing {

namespace {

constexpr char kAllowlistPopulateUmaPrefix[] =
    "SafeBrowsing.Android.RealTimeAllowlist.Populate.";
constexpr char kAllowlistIsInAllowlistUmaPrefix[] =
    "SafeBrowsing.Android.RealTimeAllowlist.IsInAllowlist.";
constexpr char kPopulateResourceFileElapsed[] =
    "SafeBrowsing.Android.RealTimeAllowlist.PopulateResourceFileElapsed";
constexpr char kIsInAllowlistElapsed[] =
    "SafeBrowsing.Android.RealTimeAllowlist.IsInAllowlistElapsed";
constexpr char kDynamicUpdate[] = "DynamicUpdate";
constexpr char kResourceBundle[] = "ResourceBundle";
const int kHashSizeInBytes = 16;
const int kInvalidVersion = -1;
const int kValidSchemeId = 0;
const size_t kMinimumHashEntryCount = 100;

}  // namespace

using base::AutoLock;

struct RealTimeUrlChecksAllowlistSingletonTrait
    : public base::DefaultSingletonTraits<RealTimeUrlChecksAllowlist> {
  static RealTimeUrlChecksAllowlist* New() {
    RealTimeUrlChecksAllowlist* instance = new RealTimeUrlChecksAllowlist();
    instance->PopulateFromResourceBundle();
    return instance;
  }
};

// static
RealTimeUrlChecksAllowlist* RealTimeUrlChecksAllowlist::instance_for_testing_ =
    nullptr;

RealTimeUrlChecksAllowlist* RealTimeUrlChecksAllowlist::GetInstance() {
  if (instance_for_testing_)
    return instance_for_testing_;
  return base::Singleton<RealTimeUrlChecksAllowlist,
                         RealTimeUrlChecksAllowlistSingletonTrait>::get();
}
void RealTimeUrlChecksAllowlist::SetInstanceForTesting(
    RealTimeUrlChecksAllowlist* instance_for_testing) {
  instance_for_testing_ = instance_for_testing;
}

RealTimeUrlChecksAllowlist::RealTimeUrlChecksAllowlist()
    : version_id_(kInvalidVersion),
      scheme_id_(kValidSchemeId),
      minimum_hash_entry_count_(kMinimumHashEntryCount) {}
RealTimeUrlChecksAllowlist::~RealTimeUrlChecksAllowlist() {
  AutoLock lock(lock_);  // DCHECK fail if the lock is held.
  instance_for_testing_ = nullptr;
}

void RealTimeUrlChecksAllowlist::PopulateFromResourceBundle() {
  AutoLock lock(lock_);
  SCOPED_UMA_HISTOGRAM_TIMER(kPopulateResourceFileElapsed);
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  std::string binary_pb =
      bundle.LoadDataResourceString(IDR_REAL_TIME_URL_CHECKS_ALLOWLIST_PB);
  RealTimeUrlChecksAllowlist::PopulateResult result =
      PopulateAllowlistFromBinaryPb(binary_pb);
  RecordPopulateMetrics(result, kResourceBundle);
  is_using_component_updater_version_ = false;
}

void RealTimeUrlChecksAllowlist::PopulateFromDynamicUpdate(
    const std::string& binary_pb) {
  AutoLock lock(lock_);
  RealTimeUrlChecksAllowlist::PopulateResult result =
      PopulateAllowlistFromBinaryPb(binary_pb);
  RecordPopulateMetrics(result, kDynamicUpdate);
  if (result == PopulateResult::kSuccess)
    is_using_component_updater_version_ = true;
}

RealTimeUrlChecksAllowlist::PopulateResult
RealTimeUrlChecksAllowlist::PopulateAllowlistFromBinaryPb(
    std::string binary_pb) {
  lock_.AssertAcquired();
  if (binary_pb.empty())
    return PopulateResult::kFailedEmpty;

  HighConfidenceAllowlist parsed_pb;
  if (!parsed_pb.ParseFromString(binary_pb))
    return PopulateResult::kFailedProtoParse;
  if (!parsed_pb.has_scheme_id())
    return PopulateResult::kFailedMissingSchemeId;
  if (parsed_pb.scheme_id() != scheme_id_)
    return PopulateResult::kSkippedInvalidSchemeId;
  if (!parsed_pb.has_version_id())
    return PopulateResult::kFailedMissingVersionId;
  if (parsed_pb.version_id() < version_id_)
    return PopulateResult::kSkippedOldVersionId;
  if (parsed_pb.version_id() == version_id_)
    return PopulateResult::kSkippedEqualVersionId;
  if (!parsed_pb.has_url_hashes())
    return PopulateResult::kFailedMissingUrlHashes;
  auto hashes_length = parsed_pb.url_hashes().length();
  if (hashes_length == 0)
    return PopulateResult::kFailedEmptyUrlHashes;
  if (hashes_length < minimum_hash_entry_count_ * kHashSizeInBytes)
    return PopulateResult::kFailedTooFewAllowlistEntries;
  if (hashes_length % kHashSizeInBytes != 0)
    return PopulateResult::kFailedDanglingHash;
  if (hashes_length >= UINT_MAX)
    return PopulateResult::kFailedHashLengthExceedsMax;

  std::set<std::string> new_allowlist_patterns;
  for (uint i = 0; i + kHashSizeInBytes <= hashes_length;
       i += kHashSizeInBytes) {
    std::string url_hash = parsed_pb.url_hashes().substr(i, kHashSizeInBytes);
    new_allowlist_patterns.insert(url_hash);
  }
  allowlist_patterns_ = new_allowlist_patterns;
  version_id_ = parsed_pb.version_id();
  return PopulateResult::kSuccess;
}

RealTimeUrlChecksAllowlist::IsInAllowlistResult
RealTimeUrlChecksAllowlist::IsInAllowlist(const GURL& url) {
  AutoLock lock(lock_);
  SCOPED_UMA_HISTOGRAM_TIMER(kIsInAllowlistElapsed);
  RealTimeUrlChecksAllowlist::IsInAllowlistResult result =
      IsInAllowlistInternal(url);
  RecordAllowlistUrlCheckMetrics(result, is_using_component_updater_version_
                                             ? kDynamicUpdate
                                             : kResourceBundle);
  return result;
}

RealTimeUrlChecksAllowlist::IsInAllowlistResult
RealTimeUrlChecksAllowlist::IsInAllowlistInternal(const GURL& url) {
  lock_.AssertAcquired();
  // If allowlist is empty, then the allowlist is unavailable
  if (allowlist_patterns_.empty()) {
    return IsInAllowlistResult::kAllowlistUnavailable;
  }

  std::vector<FullHashStr> full_hashes;
  V4ProtocolManagerUtil::UrlToFullHashes(url, &full_hashes);
  for (auto fh : full_hashes) {
    auto truncated_hash = fh.substr(0, kHashSizeInBytes);
    if (allowlist_patterns_.find(truncated_hash) != allowlist_patterns_.end()) {
      return IsInAllowlistResult::kInAllowlist;
    }
  }
  return IsInAllowlistResult::kNotInAllowlist;
}

void RealTimeUrlChecksAllowlist::SetMinimumEntryCountForTesting(
    size_t new_hash_entry_count) {
  minimum_hash_entry_count_ = new_hash_entry_count;
}

void RealTimeUrlChecksAllowlist::RecordPopulateMetrics(
    RealTimeUrlChecksAllowlist::PopulateResult result,
    const std::string& src_name) {
  lock_.AssertAcquired();
  DCHECK(src_name == kResourceBundle || src_name == kDynamicUpdate);
  base::UmaHistogramEnumeration(
      kAllowlistPopulateUmaPrefix + src_name + "Result", result);

  if (result == RealTimeUrlChecksAllowlist::PopulateResult::kSuccess) {
    base::UmaHistogramSparse(kAllowlistPopulateUmaPrefix + src_name + "Version",
                             version_id_);
    base::UmaHistogramCounts10000(
        kAllowlistPopulateUmaPrefix + src_name + "Size",
        allowlist_patterns_.size());
  }
}

void RealTimeUrlChecksAllowlist::RecordAllowlistUrlCheckMetrics(
    RealTimeUrlChecksAllowlist::IsInAllowlistResult result,
    const std::string& src_name) {
  lock_.AssertAcquired();
  DCHECK(src_name == kResourceBundle || src_name == kDynamicUpdate);
  base::UmaHistogramEnumeration(
      kAllowlistIsInAllowlistUmaPrefix + src_name + "Result", result);
}

}  // namespace safe_browsing
