// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_BROWSER_FINGERPRINTING_PROTECTION_WEB_CONTENTS_HELPER_H_
#define COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_BROWSER_FINGERPRINTING_PROTECTION_WEB_CONTENTS_HELPER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "components/privacy_sandbox/tracking_protection_settings.h"
#include "components/subresource_filter/core/browser/verified_ruleset_dealer.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

class GURL;
class PrefService;

namespace content {
class NavigationHandle;
class Page;
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace subresource_filter {
namespace mojom {
class ActivationState;
}  // namespace mojom
enum class ActivationDecision;
enum class LoadPolicy;
}  // namespace subresource_filter

namespace fingerprinting_protection_filter {

class FingerprintingProtectionObserver;
class ThrottleManager;

// Class encapsulating the logic to track refresh counts and log them to UMA
// and UKM. It also enables faking the UKM source id through `GetUkmSourceId`,
// which is otherwise difficult to do because of the factory pattern of
// FingerprintingProtectionWebContentsHelper.
class RefreshMetricsManager {
 public:
  // Linter requires an out-of-line constructor and destructor.
  RefreshMetricsManager();
  ~RefreshMetricsManager();

  // Increments the refresh count for the eTLD+1 of the given URL.
  // If the URL is invalid, this is a no-op. That doesn't matter because we only
  // care about measuring breakage on valid URLs.
  void IncrementRefreshCount(const GURL& url,
                             content::WebContents& web_contents);
  // Logs UMA and UKM metrics for each eTLD+1 that had at least one refresh in
  // the attached WebContents.
  void LogMetrics() const;

 protected:
  // Gets UKM source id for the current URL.
  // Virtual for testing.
  // `web_contents` can't be const& because the fake implementation for testing
  // needs to use a non-const method, GetURL.
  virtual ukm::SourceId GetUkmSourceId(
      content::WebContents& web_contents) const;

  // Value type of `refresh_count_by_etld_plus_one_` containing the information
  // needed to log a UMA and UKM for the refresh count of a site by eTLD+1.
  struct RefreshCountAndUkmSource {
    // UKM source ID for the URL with its eTLD+1 that was most recently
    // refreshed. This is used to log the UKM.
    ukm::SourceId last_visited_source_id;
    // Number of times the user refreshed a URL with this eTLD+1 in this
    // WebContents.
    int refresh_count;
  };
  // Maps eTLD+1 to the data required to log a UMA and UKM for the refresh
  // count. See comments for `RefreshCountAndUkmSource` for more details.
  base::flat_map<std::string, RefreshCountAndUkmSource>
      refresh_count_by_etld_plus_one_;
};

// TODO(https://crbug/346568266): Define a common interface for
// WebContentsHelpers to be used by this class and the SubresourceFilter
// version.
class FingerprintingProtectionWebContentsHelper
    : public content::WebContentsUserData<
          FingerprintingProtectionWebContentsHelper>,
      public content::WebContentsObserver {
 public:
  static void CreateForWebContents(
      content::WebContents* web_contents,
      PrefService* pref_service,
      privacy_sandbox::TrackingProtectionSettings* tracking_protection_settings,
      subresource_filter::VerifiedRulesetDealer::Handle* dealer_handle,
      bool is_incognito);

  FingerprintingProtectionWebContentsHelper(
      const FingerprintingProtectionWebContentsHelper&) = delete;
  FingerprintingProtectionWebContentsHelper& operator=(
      const FingerprintingProtectionWebContentsHelper&) = delete;

  ~FingerprintingProtectionWebContentsHelper() override;

  // Prefer to use the static methods on ThrottleManager. See comments
  // there.
  static ThrottleManager* GetThrottleManager(content::NavigationHandle& handle);
  static ThrottleManager* GetThrottleManager(content::Page& page);

  void WillDestroyThrottleManager(ThrottleManager* throttle_manager);

  // Will be called at the latest in the WillProcessResponse stage from a
  // NavigationThrottle that was registered before the throttle manager's
  // throttles created in MaybeAppendNavigationThrottles().
  void NotifyPageActivationComputed(
      content::NavigationHandle* navigation_handle,
      const subresource_filter::mojom::ActivationState& activation_state,
      const subresource_filter::ActivationDecision& activation_decision);

  // Called in WillStartRequest or WillRedirectRequest stage from a
  // ChildFrameNavigationFilteringThrottle.
  void NotifyChildFrameNavigationEvaluated(
      content::NavigationHandle* navigation_handle,
      subresource_filter::LoadPolicy load_policy);

  void NotifyOnBlockedSubresource();

  void AddObserver(FingerprintingProtectionObserver* observer);
  void RemoveObserver(FingerprintingProtectionObserver* observer);

  bool subresource_blocked_in_current_primary_page() const {
    return subresource_blocked_in_current_primary_page_;
  }
  PrefService* pref_service() const { return pref_service_; }
  privacy_sandbox::TrackingProtectionSettings* tracking_protection_settings() {
    return tracking_protection_settings_;
  }

 protected:
  explicit FingerprintingProtectionWebContentsHelper(
      content::WebContents* web_contents,
      PrefService* pref_service,
      privacy_sandbox::TrackingProtectionSettings* tracking_protection_settings,
      subresource_filter::VerifiedRulesetDealer::Handle* dealer_handle,
      bool is_incognito);

  virtual RefreshMetricsManager& GetRefreshMetricsManager();

  // content::WebContentsObserver:
  void FrameDeleted(content::FrameTreeNodeId frame_tree_node_id) override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void WebContentsDestroyed() override;

 private:
  friend class content::WebContentsUserData<
      FingerprintingProtectionWebContentsHelper>;

  // Set of frames across all pages in this WebContents that have had at least
  // one committed or aborted navigation. Keyed by FrameTreeNode ID.
  std::set<content::FrameTreeNodeId> navigated_frames_;

  // Keep track of all active throttle managers. Unowned as a throttle manager
  // will notify this class when it's destroyed so we can remove it from this
  // set.
  base::flat_set<raw_ptr<ThrottleManager, CtnExperimental>> throttle_managers_;

  // Called when a navigation starts. Creates a new throttle manager for the
  // navigation. Should only be called for a navigation in the subresource
  // filter root that's not a same-document navigation or a page-activation
  // navigation.
  void CreateThrottleManagerForNavigation(
      content::NavigationHandle* navigation_handle);

  // True iff a subresource has been blocked since the last committed
  // navigation to a new page. Set via a callback by ThrottleManager.
  bool subresource_blocked_in_current_primary_page_ = false;

  base::ObserverList<FingerprintingProtectionObserver>::Unchecked
      observer_list_;

  raw_ptr<PrefService> pref_service_;

  raw_ptr<privacy_sandbox::TrackingProtectionSettings>
      tracking_protection_settings_;

  raw_ptr<subresource_filter::VerifiedRulesetDealer::Handle> dealer_handle_;

  // Whether the  profile is in Incognito mode.
  bool is_incognito_;

  // This should only be accessed using GetRefreshCountMetricsManager,
  // because classes mocking the RefreshCountMetricsManager need to be able to
  // hide this member.
  RefreshMetricsManager refresh_metrics_manager_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace fingerprinting_protection_filter

#endif  // COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_BROWSER_FINGERPRINTING_PROTECTION_WEB_CONTENTS_HELPER_H_
