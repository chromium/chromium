// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Utilities for the SafeBrowsing DB code.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_UTIL_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_UTIL_H_

#include <stdint.h>

#include <cstring>
#include <memory>
#include <set>
#include <string>

#include "base/containers/flat_map.h"
#include "base/trace_event/traced_value.h"
#include "components/safe_browsing/core/browser/db/sb_protocol_manager_util.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"

namespace safe_browsing {

enum class ClientCallbackType : int {
  // This represents the case when we're trying to determine if a URL is
  // unsafe from the following perspectives: Malware, Phishing, UwS.
  CHECK_BROWSE_URL,

  // This represents the case when we're trying to determine if any of the
  // URLs in a vector of URLs is unsafe for downloading binaries.
  CHECK_DOWNLOAD_URLS,

  // This represents the case when we're trying to determine if a Chrome
  // extension is unsafe.
  CHECK_EXTENSION_IDS,

  // This represents the case when we're trying to determine if a URL belongs
  // to the list where subresource filter should be active.
  CHECK_URL_FOR_SUBRESOURCE_FILTER,

  // This represents the case when we're trying to determine if a URL is
  // part of the CSD allowlist.
  CHECK_CSD_ALLOWLIST,

  // This represents the case when we're trying to determine if a URL has
  // abusive notification permissions.
  CHECK_NOTIFICATION_ABUSE,

  // This represents the other cases when a check is being performed
  // synchronously so a client callback isn't required. For instance, when
  // trying to determine if an IP address is unsafe due to hosting Malware.
  CHECK_OTHER,
};

enum class SubresourceFilterType : int { ABUSIVE = 0, BETTER_ADS = 1 };

// Levels of enforcement for subresource filtering. These values must remain
// ordered by increasing severity (e.g. ENFORCE is more severe than WARN)
// because comparisons rely on this ordering.
enum class SubresourceFilterLevel : int { WARN = 0, ENFORCE = 1 };

using SubresourceFilterMatch =
    base::flat_map<SubresourceFilterType, SubresourceFilterLevel>;

// Metadata that was returned by a GetFullHash call. This is the parsed version
// of the PB (from Pver3, or Pver4 local) or JSON (from Pver4 via GMSCore).
// Some fields are only applicable to certain lists.
// When adding elements to this struct, make sure you update ToTracedValue.
struct ThreatMetadata {
  ThreatMetadata();
  ThreatMetadata(const ThreatMetadata& other);
  ThreatMetadata(ThreatMetadata&& other);
  ThreatMetadata& operator=(const ThreatMetadata& other);
  ThreatMetadata& operator=(ThreatMetadata&& other);
  ~ThreatMetadata();

  friend bool operator==(const ThreatMetadata&,
                         const ThreatMetadata&) = default;

  // Returns the metadata in a format tracing can support.
  std::unique_ptr<base::trace_event::TracedValue> ToTracedValue() const;


  // Map of list sub-types related to the SUBRESOURCE_FILTER threat type.
  SubresourceFilterMatch subresource_filter_match;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_UTIL_H_
