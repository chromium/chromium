// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_topics/browsing_topics_redirect_observer.h"

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "components/browsing_topics/browsing_topics_page_load_data_tracker.h"
#include "components/browsing_topics/test_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/common/content_features.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "content/test/test_render_view_host.h"
#include "services/network/public/cpp/features.h"

namespace browsing_topics {

class BrowsingTopicsRedirectObserverTest
    : public content::RenderViewHostTestHarness {
 public:
  BrowsingTopicsRedirectObserverTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{network::features::kBrowsingTopics},
        /*disabled_features=*/{});
  }

  ~BrowsingTopicsRedirectObserverTest() override = default;

  BrowsingTopicsPageLoadDataTracker* GetBrowsingTopicsPageLoadDataTracker() {
    return BrowsingTopicsPageLoadDataTracker::GetOrCreateForPage(
        web_contents()->GetPrimaryMainFrame()->GetPage());
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(BrowsingTopicsRedirectObserverTest, TwoNavigationsRacingCommit) {
  BrowsingTopicsRedirectObserver::MaybeCreateForWebContents(web_contents());
  auto initial_navigation =
      content::NavigationSimulator::CreateBrowserInitiated(
          GURL("https://foo.com"), web_contents());
  initial_navigation->Commit();

  GetBrowsingTopicsPageLoadDataTracker()->OnBrowsingTopicsApiUsed(
      HashedDomain(123), "abc.com", /*history_service=*/nullptr,
      /*observe=*/false);

  ukm::SourceId source_id =
      web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId();

  EXPECT_EQ(GetBrowsingTopicsPageLoadDataTracker()
                ->redirect_hosts_with_topics_invoked()
                .size(),
            1u);
  EXPECT_EQ(
      GetBrowsingTopicsPageLoadDataTracker()->source_id_before_redirects(),
      source_id);

  auto navigation1 = content::NavigationSimulator::CreateRendererInitiated(
      GURL("https://bar.com"), main_rfh());
  navigation1->SetHasUserGesture(false);
  navigation1->ReadyToCommit();

  auto navigation2 = content::NavigationSimulator::CreateRendererInitiated(
      GURL("https://baz.com"), main_rfh());
  navigation2->SetHasUserGesture(false);
  navigation2->Start();

  navigation1->Commit();

  GetBrowsingTopicsPageLoadDataTracker()->OnBrowsingTopicsApiUsed(
      HashedDomain(123), "abc.com", /*history_service=*/nullptr,
      /*observe=*/false);

  EXPECT_EQ(GetBrowsingTopicsPageLoadDataTracker()
                ->redirect_hosts_with_topics_invoked()
                .size(),
            2u);
  EXPECT_EQ(
      GetBrowsingTopicsPageLoadDataTracker()->source_id_before_redirects(),
      source_id);

  navigation2->ReadyToCommit();
  navigation2->Commit();

  GetBrowsingTopicsPageLoadDataTracker()->OnBrowsingTopicsApiUsed(
      HashedDomain(123), "abc.com", /*history_service=*/nullptr,
      /*observe=*/false);

  // The size of redirect hosts is 3 because the `navigation2` reached
  // ReadyToCommit after `navigation1` committed.
  EXPECT_EQ(GetBrowsingTopicsPageLoadDataTracker()
                ->redirect_hosts_with_topics_invoked()
                .size(),
            3u);
  EXPECT_EQ(
      GetBrowsingTopicsPageLoadDataTracker()->source_id_before_redirects(),
      source_id);
}

}  // namespace browsing_topics
