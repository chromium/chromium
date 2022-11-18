// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_ANDROID_REAL_TIME_URL_CHECKS_ALLOWLIST_H_
#define COMPONENTS_SAFE_BROWSING_ANDROID_REAL_TIME_URL_CHECKS_ALLOWLIST_H_

#include <set>
#include <string>

#include "base/synchronization/lock.h"
#include "base/timer/elapsed_timer.h"
#include "url/gurl.h"

namespace safe_browsing {

// This singleton is responsible for parsing and storing the real-time URL
// checks allowlist for use by Protego on Android. This is updated periodically
// by the component updater.
class RealTimeUrlChecksAllowlist {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused. The purpose of each value is to
  // either show that populating the result was successful or provide the
  // reason for failing to populate the allowlist. These errors range from
  // proto parsing errors to invalid pb file formatting errors.
  enum class PopulateResult {
    // Populated the new allowlist successfully.
    kSuccess = 0,
    // Input binary file is empty.
    kFailedEmpty = 1,
    // Input binary file couldn't be parsed into a HighConfidenceAllowlist
    // object.
    kFailedProtoParse = 2,
    // Failed because no URL hashes were provided.
    kFailedMissingUrlHashes = 3,
    // Failed because no version id was provided.
    kFailedMissingVersionId = 4,
    // Failed because no scheme id was provided.
    kFailedMissingSchemeId = 5,
    // Failed because the provided concatenated URL hash string is empty.
    kFailedEmptyUrlHashes = 6,
    // Failed because url_hashes is not formatted correctly. The length of the
    // string must be divisible by the size of a URL hash because the url_hashes
    // string contains the concatenated hashes of allowlisted URLs.
    kFailedDanglingHash = 7,
    // Skip populating the new allowlist because the version id is old. If this
    // happens, we will use the allowlist that was generated from the resource
    // file.
    kSkippedOldVersionId = 8,
    // Skip populating the new allowlist because the version id did not change.
    // This means that the contents of the "new" allowlist would be the same as
    // the allowlist that has already been generated.
    kSkippedEqualVersionId = 9,
    // Skip populating the new allowlist because the scheme id has changed,
    // which means the format of the binary pb is incompatible. If this happens,
    // we will use the allowlist that was generated from the resource file.
    kSkippedInvalidSchemeId = 10,
    // Failed because the length of the concatenated hashes exceeds the maximum
    // length we can work with.
    kFailedHashLengthExceedsMax = 11,
    // Failed because there are not enough URL hashes provided to generate the
    // new allowlist. The minimum number of URL hashes required for populating
    // is 100, but this number can be changed for testing.
    kFailedTooFewAllowlistEntries = 12,
    kMaxValue = kFailedTooFewAllowlistEntries,
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class IsInAllowlistResult {
    kNotInAllowlist = 0,
    kInAllowlist = 1,
    kAllowlistUnavailable = 2,
    kMaxValue = kAllowlistUnavailable,
  };

  virtual ~RealTimeUrlChecksAllowlist();

  static RealTimeUrlChecksAllowlist* GetInstance();  // Singleton

  static void SetInstanceForTesting(
      RealTimeUrlChecksAllowlist* instance_for_testing);

  // Updates the internal allowlist from a binary proto fetched from the
  // component updater.
  virtual void PopulateFromDynamicUpdate(const std::string& binary_pb);

  // Returns whether the provided URL has a match in the local allowlist.
  IsInAllowlistResult IsInAllowlist(const GURL& url);

  mutable base::Lock lock_;

 protected:
  RealTimeUrlChecksAllowlist();

 private:
  friend class RealTimeUrlChecksAllowlistResourceFileTest;
  friend struct RealTimeUrlChecksAllowlistSingletonTrait;
  friend class RealTimeUrlChecksAllowlistTest;

  // Updates the internal allowlist from the local resource file.
  void PopulateFromResourceBundle();

  // Validate the binary_pb contents and if valid, update the allowlist
  // URL hashes and version.
  virtual PopulateResult PopulateAllowlistFromBinaryPb(std::string binary_pb);

  // Searches the allowlist_patterns_for URL hash.
  IsInAllowlistResult IsInAllowlistInternal(const GURL& url);

  // Sets the minimum_hash_entry_count_ value so that we don't need an
  // allowlist with 100+ entries for testing.
  void SetMinimumEntryCountForTesting(size_t new_hash_entry_count);

  // Record UMA data relevant for populating the allowlist.
  virtual void RecordPopulateMetrics(PopulateResult result,
                                     const std::string& src_name);

  // Record UMA data relevant for checking the allowlist for a URL.
  virtual void RecordAllowlistUrlCheckMetrics(IsInAllowlistResult result,
                                              const std::string& src_name);

  static RealTimeUrlChecksAllowlist* instance_for_testing_;

  // Set of URL hash prefixes in the safe browsing allowlist.
  // We are not using a flat_set because these are ideal for small sets, but
  // the allowlist contains roughly 2,000 strings so we are using std::set.
  std::set<std::string> allowlist_patterns_;

  // Version of the allowlist, same as version_id in realtimeallowlist.proto.
  int version_id_;

  // Scheme id of the allowlist, same as scheme_id in realtimeallowlist.proto.
  // This attribute is used for backwards compatibility. If there are breaking
  // changes to the code, then the kValidSchemeId value (the default scheme id)
  // should be modified in the code.
  int scheme_id_;

  // Minimum number of hash entries required for the allowlist to be valid.
  size_t minimum_hash_entry_count_;

  // True if we have successfully fetched the dynamic resource for the allowlist
  // and populated the allowlist with it.
  bool is_using_component_updater_version_ = false;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_ANDROID_REAL_TIME_URL_CHECKS_ALLOWLIST_H_
