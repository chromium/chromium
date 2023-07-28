// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CORE_HTTPS_ONLY_MODE_ALLOWLIST_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CORE_HTTPS_ONLY_MODE_ALLOWLIST_H_

#include <set>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "url/gurl.h"

namespace base {
class Clock;
}

namespace security_interstitials {

// Stores allowlist decisions for HTTPS-Only Mode.
// A user can allowlist a site by clicking through its HTTPS-Only Mode
// interstitial. For default storage partitions (e.g. non-incognito mode), the
// decision is stored in content settings. Otherwise, it's stored in memory.
class HttpsOnlyModeAllowlist {
 public:
  HttpsOnlyModeAllowlist(HostContentSettingsMap* host_content_settings_map,
                         base::Clock* clock,
                         base::TimeDelta expiration_timeout);

  HttpsOnlyModeAllowlist(const HttpsOnlyModeAllowlist&) = delete;
  HttpsOnlyModeAllowlist& operator=(const HttpsOnlyModeAllowlist&) = delete;

  ~HttpsOnlyModeAllowlist();

  // Adds host to the list of sites allowed to load over HTTP.
  void AllowHttpForHost(const std::string& host, bool is_nondefault_storage);

  // Returns true if host is allowed to be loaded over HTTP. If so, we don't
  // attempt to upgrade it to HTTPS.
  bool IsHttpAllowedForHost(const std::string& host,
                            bool is_nondefault_storage) const;
  bool IsHttpAllowedForAnyHost(bool is_nondefault_storage) const;

  // Revokes all HTTP exceptions made by the user for host.
  void RevokeUserAllowExceptions(const std::string& host);

  // Clears allowlist for the given pattern filter. If the pattern filter is
  // empty, clears allowlist for all hosts.
  void Clear(
      base::Time delete_begin,
      base::Time delete_end,
      const HostContentSettingsMap::PatternSourcePredicate& pattern_filter);

  // Clears the persistent and in-memory allowlist entries. All of in-memory
  // entries are removed, but only persistent entries between delete_begin and
  // delete_end are removed.
  void ClearAllowlist(base::Time delete_begin, base::Time delete_end);

  // Sets the test clock.
  void SetClockForTesting(base::Clock* clock);

 private:
  raw_ptr<HostContentSettingsMap> host_content_settings_map_;
  raw_ptr<base::Clock> clock_;
  base::TimeDelta expiration_timeout_;

  // Tracks sites that are allowed to load over HTTP when HTTPS-First Mode is
  // enabled, for non-default storage partitions. Allowed hosts are exact
  // hostname matches -- subdomains of a host on the allowlist must be
  // separately allowlisted.
  //
  // In most cases, HTTP interstitial decisions are stored in ContentSettings
  // and persisted to disk, like cert decisions. Similar to cert decisions, for
  // non-default StoragePartitions the decisions should be isolated from normal
  // browsing and don't need to be persisted to disk. For these cases, track
  // allowlist decisions purely in memory.
  std::set<std::string /* host */>
      allowed_http_hosts_for_non_default_storage_partitions_;
};

}  // namespace security_interstitials

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CORE_HTTPS_ONLY_MODE_ALLOWLIST_H_
