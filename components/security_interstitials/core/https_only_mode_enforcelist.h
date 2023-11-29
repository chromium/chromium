// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CORE_HTTPS_ONLY_MODE_ENFORCELIST_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CORE_HTTPS_ONLY_MODE_ENFORCELIST_H_

#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "url/gurl.h"

namespace base {
class Clock;
}

namespace security_interstitials {

// Stores enforcelist decisions for HTTPS-First Mode. HFM can get enforced on
// a site if its site engagement scores indicate that the user interacts with
// its HTTPS URLs much more than its HTTP URLs.
// For default storage partitions (e.g. non-incognito mode), the
// decision is stored in content settings. Otherwise, it's stored in memory.
class HttpsOnlyModeEnforcelist {
 public:
  explicit HttpsOnlyModeEnforcelist(
      HostContentSettingsMap* host_content_settings_map,
      base::Clock* clock);

  HttpsOnlyModeEnforcelist(const HttpsOnlyModeEnforcelist&) = delete;
  HttpsOnlyModeEnforcelist& operator=(const HttpsOnlyModeEnforcelist&) = delete;

  ~HttpsOnlyModeEnforcelist();

  // Adds host to the list of sites not allowed to load over HTTP. Should only
  // be called if HFM isn't already enforced for this host.
  void EnforceForHost(const std::string& host, bool is_nondefault_storage);

  // Marks host so that HTTPS-First Mode isn't enabled on it. Doesn't actually
  // remove the website setting for the host, so that we can record metrics
  // about accumulated host count. Should only be called if HFM is already
  // enforced for this host.
  void UnenforceForHost(const std::string& host, bool is_nondefault_storage);

  // Returns true if URL is not allowed to be loaded over HTTP. This check
  // ignores the scheme of the URL: calling it with http:// or https:// will
  // return the same result.
  bool IsEnforcedForUrl(const GURL& url, bool is_nondefault_storage) const;

  // Revokes all HTTPS enforcements for host.
  void RevokeEnforcements(const std::string& host);

  std::set<GURL> GetHosts(bool is_nondefault_storage) const;

  // Clears enforcelist for the given pattern filter. If the pattern filter is
  // empty, clears enforcelist for all hosts.
  void Clear(
      base::Time delete_begin,
      base::Time delete_end,
      const HostContentSettingsMap::PatternSourcePredicate& pattern_filter);

  // Clears the persistent and in-memory enforcelist entries. All of in-memory
  // entries are removed, but only persistent entries between delete_begin and
  // delete_end are removed.
  void ClearEnforcements(base::Time delete_begin, base::Time delete_end);

  // Sets the test clock.
  void SetClockForTesting(base::Clock* clock);

 private:
  // Records metrics about the host list.
  void RecordMetrics(bool is_nondefault_storage);

  raw_ptr<HostContentSettingsMap> host_content_settings_map_;
  raw_ptr<base::Clock> clock_;

  // Tracks sites that are not allowed to load over HTTP, for non-default
  // storage partitions. Enforced hosts are exact hostname matches -- subdomains
  // of a host on the enforcelist must be separately added.
  //
  // In most cases, HTTP interstitial decisions are stored in ContentSettings
  // and persisted to disk, like cert decisions. Similar to cert decisions, for
  // non-default StoragePartitions the decisions should be isolated from normal
  // browsing and don't need to be persisted to disk. For these cases, track
  // enforcelist decisions purely in memory.
  std::set<std::string /* host */>
      enforce_https_hosts_for_non_default_storage_partitions_;
};

}  // namespace security_interstitials

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CORE_HTTPS_ONLY_MODE_ENFORCELIST_H_
