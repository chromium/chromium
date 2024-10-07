// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_BROWSER_FINGERPRINTING_PROTECTION_WEB_CONTENTS_HELPER_H_
#define COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_BROWSER_FINGERPRINTING_PROTECTION_WEB_CONTENTS_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "components/privacy_sandbox/tracking_protection_settings.h"
#include "components/subresource_filter/core/browser/verified_ruleset_dealer.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

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

  void NotifyOnBlockedResources();

  void AddObserver(FingerprintingProtectionObserver* observer);
  void RemoveObserver(FingerprintingProtectionObserver* observer);

  bool is_subresource_blocked() const { return is_subresource_blocked_; }
  PrefService* pref_service() const { return pref_service_; }
  privacy_sandbox::TrackingProtectionSettings* tracking_protection_settings() {
    return tracking_protection_settings_;
  }

 protected:
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
  explicit FingerprintingProtectionWebContentsHelper(
      content::WebContents* web_contents,
      PrefService* pref_service,
      privacy_sandbox::TrackingProtectionSettings* tracking_protection_settings,
      subresource_filter::VerifiedRulesetDealer::Handle* dealer_handle,
      bool is_incognito);

  void Detach();

  friend class content::WebContentsUserData<
      FingerprintingProtectionWebContentsHelper>;

  // Set of frames across all pages in this WebContents that have had at least
  // one committed or aborted navigation. Keyed by FrameTreeNode ID.
  std::set<content::FrameTreeNodeId> navigated_frames_;

  // Keep track of all active throttle managers. Unowned as a throttle manager
  // will notify this class when it's destroyed so we can remove it from this
  // set.
  base::flat_set<raw_ptr<ThrottleManager, CtnExperimental>> throttle_managers_;

  bool is_subresource_blocked_ = false;

  // Tracks refreshes observed.
  int refresh_count_ = 0;

  base::ObserverList<FingerprintingProtectionObserver>::Unchecked
      observer_list_;

  // TODO(https://crbug.com/40280666): Triage dangling pointers.
  raw_ptr<PrefService, DanglingUntriaged> pref_service_;

  raw_ptr<privacy_sandbox::TrackingProtectionSettings, DanglingUntriaged>
      tracking_protection_settings_;

  raw_ptr<subresource_filter::VerifiedRulesetDealer::Handle, DanglingUntriaged>
      dealer_handle_;

  // Whether the  profile is in Incognito mode.
  bool is_incognito_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace fingerprinting_protection_filter

#endif  // COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_BROWSER_FINGERPRINTING_PROTECTION_WEB_CONTENTS_HELPER_H_
