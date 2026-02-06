// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_FAMILY_LINK_URL_FILTER_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_FAMILY_LINK_URL_FILTER_H_

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/callback_list.h"
#include "base/memory/raw_ref.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "components/safe_search_api/url_checker.h"
#include "components/supervised_user/core/browser/family_link_settings_service.h"
#include "components/supervised_user/core/browser/supervised_user_url_filtering_service.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "url/gurl.h"

class PrefService;

namespace supervised_user {

// This URL filter implementation manages the filtering behavior for URLs as
// configured in the Family Link system, i.e. it tells callers if a URL should
// be allowed or blocked. It the following information from the Family Link:
//   1) User-specified manual overrides (allow or block) for either sites
//     (hostnames) or exact URLs,
//   2) Then, depending on the mode of operation:
//      a) without asynchronous check request, the default behavior (allow or
//         block) is used,
//      b) with asynchronous check request, the url is tested against remote
//         service.
//
// Since the instance of this filter is in 1-1 relation with the supervised
// user service, it is present all the time. However, when parental controls
// are off or filtering is not requested, the filter operates in a disabled
// state which transparently classifies all urls as allowed.
class FamilyLinkUrlFilter : public UrlFilteringDelegate {
 public:
  // This enum describes whether the approved list or blocked list is used on
  // Chrome on Chrome OS, which is set by Family Link App or at
  // families.google.com/families via "manage sites" setting. This is also
  // referred to as manual behavior/filter as parent need to add everything one
  // by one. These values are logged to UMA. Entries should not be renumbered
  // and numeric values should never be reused. Please keep in sync with
  // "FamilyLinkManagedSiteList" in src/tools/metrics/histograms/enums.xml.
  enum class ManagedSiteList {
    // The web filter has both empty blocked and approved list.
    kEmpty = 0,

    // The web filter has approved list only.
    kApprovedListOnly = 1,

    // The web filter has blocked list only.
    kBlockedListOnly = 2,

    // The web filter has both approved list and blocked list.
    kBoth = 3,

    // Used for UMA. Update kMaxValue to the last value. Add future entries
    // above this comment. Sync with enums.xml.
    kMaxValue = kBoth,
  };

  // This enum describes the kind of conflicts between allow and block list
  // entries that match a given input host and resolve to different filtering
  // results.
  // They distinguish between conflicts:
  // 1) entirely due to trivial subdomain differences,
  // 2) due to differences other than the trivial subdomain and
  // 3) due to both kinds of differences.
  // These values are logged to UMA. Entries should not be renumbered and
  // numeric values should never be reused. Please keep in sync with
  // "FamilyLinkFilteringSubdomainConflictType" in
  // src/tools/metrics/histograms/enums.xml.
  enum class FilteringSubdomainConflictType {
    kTrivialSubdomainConflictOnly = 0,
    kOtherConflictOnly = 1,
    kTrivialSubdomainConflictAndOtherConflict = 2,
    kMaxValue = kTrivialSubdomainConflictAndOtherConflict,
  };

  // Encapsulates statistics about this URL filter.
  struct Statistics {
    bool operator==(const Statistics& other) const = default;
    ManagedSiteList GetManagedSiteList() const;

    std::size_t allowed_hosts_count = 0;
    std::size_t blocked_hosts_count = 0;
    std::size_t allowed_urls_count = 0;
    std::size_t blocked_urls_count = 0;
  };

  // Provides access to functionality from services on which we don't want
  // to depend directly.
  class Delegate {
   public:
    virtual ~Delegate() = default;
    // Returns true if the webstore extension URL is eligible for downloading
    // for a supervised user managed by Family Link.
    virtual bool SupportsWebstoreURL(const GURL& url) const = 0;
  };

  FamilyLinkUrlFilter(
      FamilyLinkSettingsService& family_link_settings_service,
      const PrefService& user_prefs,
      std::unique_ptr<Delegate> delegate,
      std::unique_ptr<safe_search_api::URLCheckerClient> url_checker_client);

  ~FamilyLinkUrlFilter() override;

  static const char* GetManagedSiteListConflictHistogramNameForTest();
  static const char* GetManagedSiteListConflictTypeHistogramNameForTest();

  // Returns true if the |host| matches the pattern. A pattern is a hostname
  // with one or both of the following modifications:
  // - If the pattern starts with "*.", it matches the host or any subdomain
  //   (e.g. the pattern "*.google.com" would match google.com, www.google.com,
  //   or accounts.google.com).
  // - If the pattern ends with ".*", it matches the host on any known TLD
  //   (e.g. the pattern "google.*" would match google.com or google.co.uk).
  // If the |host| starts with "www." but the |pattern| starts with neither
  // "www." nor "*.", the function strips the "www." part of |host| and tries to
  // match again. See the SupervisedUserURLFilterTest.HostMatchesPattern unit
  // test for more examples. Asterisks in other parts of the pattern are not
  // allowed. |host| and |pattern| are assumed to be normalized to lower-case.
  // This method is public for testing.
  static bool HostMatchesPattern(const std::string& canonical_host,
                                 const std::string& pattern);

  // Refreshes data structures that hold manually configured url and host
  // exceptions.
  void UpdateManualHosts();
  void UpdateManualUrls();

  // Returns summary of url filtering settings.
  Statistics GetFilteringStatistics() const;

  // Substitutes the URL filter for testing. For use where TestingFactory cant
  // substitute the checker client.
  void SetURLCheckerClientForTesting(
      std::unique_ptr<safe_search_api::URLCheckerClient> url_checker_client);

  // Returns the URL that should be sent for remote approvals to ensure that
  // the url in the filtering result will no longer trigger interstitial.
  // This methods prefers unnormalized url if it is already present in the block
  // list: this way, the Family Link backend will remove this entry from the
  // block list and add one to the allow list. Otherwise, a normalized url is
  // returned.
  // TODO(crbug.com/475731807): This method is Family-Link specific, and
  // probably should live in the SupervisedUserSettingsService after it's
  // renamed to FamilyLinkUserSettingsService.
  GURL GetEffectiveUrlToUnblock(WebFilteringResult result) const;

  // UrlFilteringDelegate:
  WebFilterType GetWebFilterType() const override;
  WebFilteringResult GetFilteringBehavior(const GURL& url) const override;
  void GetFilteringBehavior(const GURL& url,
                            bool skip_manual_parent_filter,
                            WebFilteringResult::Callback callback,
                            const WebFilterMetricsOptions& options) override;
  void GetFilteringBehaviorForSubFrame(
      const GURL& url,
      const GURL& main_frame_url,
      WebFilteringResult::Callback callback,
      const WebFilterMetricsOptions& options) override;
  std::string_view GetName() const override;

 private:
  // Allows proxying deprecated calls to the filter for the time of migration.
  friend class SupervisedUserUrlFilteringService;

  bool IsExemptedFromGuardianApproval(const GURL& effective_url) const;

  void RunAsyncChecker(const GURL& url, WebFilteringResult::Callback callback);

  FilteringBehavior GetManualFilteringBehaviorForURL(const GURL& url) const;

  // Calculates a URL that should unblock the filtering result but without the
  // normalization it (eg. stripping username, password, query params, ref).
  GURL GetUnnormalizedEffectiveUrlToUnblock(WebFilteringResult result) const;

  void OnFamilyLinkSettingsChanged(const base::DictValue& settings);

  // Maps from a URL to whether it is manually allowed (true) or blocked
  // (false).
  std::map<GURL, bool> url_map_;

  // Blocked and Allowed host lists.
  std::set<std::string, std::less<>> blocked_host_list_;
  std::set<std::string, std::less<>> allowed_host_list_;

  // Statistics about this filter configuration
  Statistics statistics_;

  // Two sources of settings: directly from FamilyLinkSettingsService or through
  // PrefService (deprecated, guarded by feature flag).
  raw_ref<const FamilyLinkSettingsService> family_link_settings_service_;
  raw_ref<const PrefService> user_prefs_;

  std::unique_ptr<Delegate> delegate_;

  std::unique_ptr<safe_search_api::URLChecker> async_url_checker_;

  base::CallbackListSubscription family_link_settings_subscription_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_FAMILY_LINK_URL_FILTER_H_
