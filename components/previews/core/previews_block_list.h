// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREVIEWS_CORE_PREVIEWS_BLOCK_LIST_H_
#define COMPONENTS_PREVIEWS_CORE_PREVIEWS_BLOCK_LIST_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "components/blocklist/opt_out_blocklist/opt_out_blocklist.h"
#include "components/blocklist/opt_out_blocklist/opt_out_blocklist_delegate.h"
#include "components/blocklist/opt_out_blocklist/opt_out_store.h"
#include "components/previews/core/previews_experiments.h"

class GURL;

namespace base {
class Clock;
}

namespace previews {

// Must remain synchronized with |PreviewsEligibilityReason| in
// //tools/metrics/histograms/enums.xml.
enum class PreviewsEligibilityReason {
  // The preview navigation was allowed.
  ALLOWED = 0,
  // The block list was not initialized.
  BLOCKLIST_UNAVAILABLE = 1,
  // The block list has not loaded from disk yet.
  BLOCKLIST_DATA_NOT_LOADED = 2,
  // The user has opted out of a preview recently.
  USER_RECENTLY_OPTED_OUT = 3,
  // The user has opted out of previews often, and is no longer shown previews
  // on any host.
  USER_BLOCKLISTED = 4,
  // The user has opted out of previews on a specific host often, and was not
  // not shown a previews on that host.
  HOST_BLOCKLISTED = 5,
  // The network quality estimate is not available.
  NETWORK_QUALITY_UNAVAILABLE = 6,
  // The network was fast enough to not warrant previews.
  NETWORK_NOT_SLOW = 7,
  // If the page was reloaded, the user should not be shown a stale preview.
  RELOAD_DISALLOWED = 8,
  // DEPRECATED: The host is explicitly blocklisted by the server, so the user
  // was not shown
  // a preview.
  // Replaced by NOT_ALLOWED_BY_OPTIMIZATION_GUIDE.
  DEPRECATED_HOST_BLOCKLISTED_BY_SERVER = 9,
  // DEPRECATED: The host is not allowlisted by the server for a preview
  // decision that uses
  // server optimization hints.
  // Replaced by NOT_ALLOWED_BY_OPTIMIZATION_GUIDE.
  DEPRECATED_HOST_NOT_ALLOWLISTED_BY_SERVER = 10,
  // The preview is allowed but without an expected check of server optimization
  // hints because they are not enabled (features::kOptimizationHints).
  ALLOWED_WITHOUT_OPTIMIZATION_HINTS = 11,
  // The preview type chosen as the committed preview.
  COMMITTED = 12,
  // Previews blocked by a Cache-Control:no-transform directive.
  CACHE_CONTROL_NO_TRANSFORM = 13,
  // The network is faster than the max slow page triggering threshold for the
  // session. No longer used as of M80.
  DEPRECATED_NETWORK_NOT_SLOW_FOR_SESSION = 14,
  // Device is offline.
  DEVICE_OFFLINE = 15,
  // URL contained Basic Authentication, i.e.: a username or password.
  URL_HAS_BASIC_AUTH = 16,
  // Optimization hints needed to be checked for this preview type, but were not
  // available. Common on first navigations.
  OPTIMIZATION_HINTS_NOT_AVAILABLE = 17,
  // The navigation URL has a media suffix which is excluded from previews.
  EXCLUDED_BY_MEDIA_SUFFIX = 18,
  // The Optimization Guide was checked for this preview type and the
  // optimization guide did not allow this preview type.
  NOT_ALLOWED_BY_OPTIMIZATION_GUIDE = 19,
  // The preview was not performed due to a coinflip experiment holdback.
  COINFLIP_HOLDBACK = 20,
  // A redirect loop was detected.
  REDIRECT_LOOP_DETECTED = 21,
  // URL matched the deny list.
  DENY_LIST_MATCHED = 22,
  // The page load was not predicted to be painful.
  PAGE_LOAD_PREDICTION_NOT_PAINFUL = 23,
  LAST,
};

// Manages the state of block listed domains for the previews experiment. Loads
// the stored block list from |opt_out_store| and manages an in memory block
// list on the IO thread. Updates to the block list are stored in memory and
// pushed to the store. Asynchronous modifications are stored in a queue and
// executed in order. Reading from the block list is always synchronous, and if
// the block list is not currently loaded (e.g., at startup, after clearing
// browsing history), domains are reported as block listed. The list stores no
// more than previews::params::MaxInMemoryHostsInBlockList hosts in-memory,
// which defaults to 100.
class PreviewsBlockList : public blocklist::OptOutBlocklist {
 public:
  PreviewsBlockList(
      std::unique_ptr<blocklist::OptOutStore> opt_out_store,
      base::Clock* clock,
      blocklist::OptOutBlocklistDelegate* blocklist_delegate,
      blocklist::BlocklistData::AllowedTypesAndVersions allowed_types);
  ~PreviewsBlockList() override;

  // Asynchronously adds a new navigation to to the in-memory block list and
  // backing store. |opt_out| is whether the user opted out of the preview or
  // navigated away from the page without opting out. |type| is only passed to
  // the backing store. If the in memory map has reached the max number of hosts
  // allowed, and |url| is a new host, a host will be evicted based on recency
  // of the hosts most recent opt out. It returns the time used for recording
  // the moment when the navigation is added for logging.
  base::Time AddPreviewNavigation(const GURL& url,
                                  bool opt_out,
                                  PreviewsType type);

  // Synchronously determines if |host_name| should be allowed to show previews.
  // Returns the reason the blocklist disallowed the preview, or
  // PreviewsEligibilityReason::ALLOWED if the preview is allowed. Record
  // checked reasons in |passed_reasons|. Virtualized in testing.
  virtual PreviewsEligibilityReason IsLoadedAndAllowed(
      const GURL& url,
      PreviewsType type,
      std::vector<PreviewsEligibilityReason>* passed_reasons) const;

 protected:
  // blocklist::OptOutBlocklist (virtual for testing):
  bool ShouldUseSessionPolicy(base::TimeDelta* duration,
                              size_t* history,
                              int* threshold) const override;
  bool ShouldUsePersistentPolicy(base::TimeDelta* duration,
                                 size_t* history,
                                 int* threshold) const override;
  bool ShouldUseHostPolicy(base::TimeDelta* duration,
                           size_t* history,
                           int* threshold,
                           size_t* max_hosts) const override;
  bool ShouldUseTypePolicy(base::TimeDelta* duration,
                           size_t* history,
                           int* threshold) const override;
  blocklist::BlocklistData::AllowedTypesAndVersions GetAllowedTypes()
      const override;

 private:
  const blocklist::BlocklistData::AllowedTypesAndVersions allowed_types_;

  DISALLOW_COPY_AND_ASSIGN(PreviewsBlockList);
};

}  // namespace previews

#endif  // COMPONENTS_PREVIEWS_CORE_PREVIEWS_BLOCK_LIST_H_
