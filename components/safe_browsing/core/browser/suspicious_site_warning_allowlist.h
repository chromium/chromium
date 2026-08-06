// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_SUSPICIOUS_SITE_WARNING_ALLOWLIST_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_SUSPICIOUS_SITE_WARNING_ALLOWLIST_H_

#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"

namespace safe_browsing {


// Stores local allowlist decisions for Suspicious Site Warnings (SSW).
// When a user clicks "Mark as Safe" (or "Continue anyway") on an SSW warning,
// the host is added to this local allowlist for a 30-day TTL.
// While allowlisted, SSW warnings for that host will not be displayed.
//
// A host is marked as allowlisted in the HostContentSettingsMap if a
// dictionary value is present. Expired entries are automatically filtered
// out by the framework via ContentSettingConstraints.
class SuspiciousSiteWarningAllowlist {
 public:
  static constexpr base::TimeDelta kDefaultExpirationTimeout = base::Days(30);

  explicit SuspiciousSiteWarningAllowlist(
      scoped_refptr<HostContentSettingsMap> host_content_settings_map);
  SuspiciousSiteWarningAllowlist(
      scoped_refptr<HostContentSettingsMap> host_content_settings_map,
      base::TimeDelta expiration_timeout);

  SuspiciousSiteWarningAllowlist(const SuspiciousSiteWarningAllowlist&) =
      delete;
  SuspiciousSiteWarningAllowlist& operator=(
      const SuspiciousSiteWarningAllowlist&) = delete;

  ~SuspiciousSiteWarningAllowlist();

  // Adds host to the local SSW allowlist.
  void AllowSiteForHost(const std::string& host);

  // Returns true if host is in the local SSW allowlist and has not expired.
  bool IsSiteAllowedForHost(const std::string& host) const;

  // Revokes the allowlist entry for a specific host.
  void RevokeUserAllowException(const std::string& host);

  // Clears allowlist entries for the specified time range and pattern filter.
  void Clear(
      base::Time delete_begin,
      base::Time delete_end,
      const HostContentSettingsMap::PatternSourcePredicate& pattern_filter);

  // Clears allowlist entries for the specified time range.
  void Clear(base::Time delete_begin, base::Time delete_end);

 private:
  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;
  base::TimeDelta expiration_timeout_;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_SUSPICIOUS_SITE_WARNING_ALLOWLIST_H_
