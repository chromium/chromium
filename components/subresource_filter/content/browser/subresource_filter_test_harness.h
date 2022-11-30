// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SUBRESOURCE_FILTER_TEST_HARNESS_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SUBRESOURCE_FILTER_TEST_HARNESS_H_

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "components/subresource_filter/content/browser/fake_safe_browsing_database_manager.h"
#include "components/subresource_filter/content/browser/throttle_manager_test_support.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/subresource_filter/core/browser/subresource_filter_features_test_support.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/test_renderer_host.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/messages/android/mock_message_dispatcher_bridge.h"
#endif

class GURL;

namespace content {
class NavigationThrottle;
class RenderFrameHost;
}  // namespace content

namespace infobars {
class ContentInfoBarManager;
}

namespace subresource_filter {

class RulesetService;
class SubresourceFilterContentSettingsManager;

// Unit test harness for the subresource filtering component.
class SubresourceFilterTestHarness : public content::RenderViewHostTestHarness,
                                     public content::WebContentsObserver {
 public:
  // Allowlist rules must prefix a disallowed rule in order to work correctly.
  static constexpr const char kDefaultAllowedSuffix[] = "not_disallowed.html";
  static constexpr const char kDefaultDisallowedSuffix[] = "disallowed.html";
  static constexpr const char kDefaultDisallowedUrl[] =
      "https://example.test/disallowed.html";

  SubresourceFilterTestHarness();
  ~SubresourceFilterTestHarness() override;

  SubresourceFilterTestHarness(const SubresourceFilterTestHarness&) = delete;
  SubresourceFilterTestHarness& operator=(const SubresourceFilterTestHarness&) =
      delete;

  // content::RenderViewHostTestHarness:
  void SetUp() override;
  void TearDown() override;

  // Returns the frame host the navigation commit in, or nullptr if it did not
  // succeed.
  content::RenderFrameHost* SimulateNavigateAndCommit(
      const GURL& url,
      content::RenderFrameHost* rfh);

  // Creates a subframe as a child of |parent|, and navigates it to a URL
  // disallowed by the default ruleset (kDefaultDisallowedUrl). Returns the
  // frame host the navigation commit in, or nullptr if it did not succeed.
  content::RenderFrameHost* CreateAndNavigateDisallowedSubframe(
      content::RenderFrameHost* parent);

  void ConfigureAsSubresourceFilterOnlyURL(const GURL& url);

  void RemoveURLFromBlocklist(const GURL& url);

  SubresourceFilterContentSettingsManager* GetSettingsManager();

  testing::ScopedSubresourceFilterConfigurator& scoped_configuration() {
    return scoped_configuration_;
  }

  FakeSafeBrowsingDatabaseManager* fake_safe_browsing_database() {
    return database_manager_.get();
  }

  void SetIsAdFrame(content::RenderFrameHost* render_frame_host,
                    bool is_ad_frame);

  content::WebContents* web_contents() {
    return content::RenderViewHostTestHarness::web_contents();
  }

 protected:
  // Tests can override this to have custom throttles added to navigations.
  virtual void AppendCustomNavigationThrottles(
      content::NavigationHandle* navigation_handle,
      std::vector<std::unique_ptr<content::NavigationThrottle>>* throttles) {}

  sync_preferences::TestingPrefServiceSyncable* pref_service() {
    return &pref_service_;
  }

 private:
  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;

  base::ScopedTempDir ruleset_service_dir_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  testing::ScopedSubresourceFilterConfigurator scoped_configuration_;
  scoped_refptr<FakeSafeBrowsingDatabaseManager> database_manager_;
  std::unique_ptr<ThrottleManagerTestSupport> throttle_manager_test_support_;
  std::unique_ptr<infobars::ContentInfoBarManager> infobar_manager_;
  std::unique_ptr<RulesetService> ruleset_service_;
#if BUILDFLAG(IS_ANDROID)
  messages::MockMessageDispatcherBridge message_dispatcher_bridge_;
#endif
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SUBRESOURCE_FILTER_TEST_HARNESS_H_
