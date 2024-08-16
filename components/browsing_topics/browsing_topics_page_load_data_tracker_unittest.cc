// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_topics/browsing_topics_page_load_data_tracker.h"

#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "components/browsing_topics/test_util.h"
#include "components/history/content/browser/history_context_helper.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/test_history_database.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browsing_topics_test_util.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "content/test/test_render_view_host.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/permissions_policy/origin_with_possible_wildcards.h"

namespace browsing_topics {

class BrowsingTopicsPageLoadDataTrackerTest
    : public content::RenderViewHostTestHarness {
 public:
  BrowsingTopicsPageLoadDataTrackerTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{blink::features::kBrowsingTopics},
        /*disabled_features=*/{});

    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());

    history_service_ = std::make_unique<history::HistoryService>();
    history_service_->Init(
        history::TestHistoryDatabaseParamsForPath(temp_dir_.GetPath()));
  }

  ~BrowsingTopicsPageLoadDataTrackerTest() override = default;

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    // The test assumes pages gets deleted after navigation, triggering metrics
    // recording. Disable back/forward cache to ensure that pages don't get
    // preserved in the cache.
    content::DisableBackForwardCacheForTesting(
        web_contents(), content::BackForwardCache::TEST_REQUIRES_NO_CACHING);
  }

  void TearDown() override {
    DCHECK(history_service_);

    base::RunLoop run_loop;
    history_service_->SetOnBackendDestroyTask(run_loop.QuitClosure());
    history_service_.reset();
    run_loop.Run();

    content::RenderViewHostTestHarness::TearDown();
  }

  void NavigateToPage(const GURL& url,
                      bool publicly_routable,
                      bool browsing_topics_permissions_policy_allowed,
                      bool interest_cohort_permissions_policy_allowed,
                      bool browser_initiated = true,
                      bool has_user_gesture = false,
                      bool add_same_document_nav = false) {
    std::unique_ptr<content::NavigationSimulator> simulator;
    if (browser_initiated) {
      simulator = content::NavigationSimulator::CreateBrowserInitiated(
          url, web_contents());
    } else {
      simulator = content::NavigationSimulator::CreateRendererInitiated(
          url, main_rfh());
    }
    simulator->SetHasUserGesture(has_user_gesture);

    if (!publicly_routable) {
      net::IPAddress address;
      EXPECT_TRUE(address.AssignFromIPLiteral("0.0.0.0"));
      simulator->SetSocketAddress(net::IPEndPoint(address, /*port=*/0));
    }

    blink::ParsedPermissionsPolicy policy;

    if (!browsing_topics_permissions_policy_allowed) {
      policy.emplace_back(
          blink::mojom::PermissionsPolicyFeature::kBrowsingTopics,
          /*allowed_origins=*/std::vector<blink::OriginWithPossibleWildcards>(),
          /*self_if_matches=*/std::nullopt,
          /*matches_all_origins=*/false,
          /*matches_opaque_src=*/false);
    }

    if (!interest_cohort_permissions_policy_allowed) {
      policy.emplace_back(
          blink::mojom::PermissionsPolicyFeature::
              kBrowsingTopicsBackwardCompatible,
          /*allowed_origins=*/std::vector<blink::OriginWithPossibleWildcards>(),
          /*self_if_matches=*/std::nullopt,
          /*matches_all_origins=*/false,
          /*matches_opaque_src=*/false);
    }

    simulator->SetPermissionsPolicyHeader(std::move(policy));

    simulator->Commit();
    if (add_same_document_nav) {
      content::NavigationSimulator::CreateRendererInitiated(url, main_rfh())
          ->CommitSameDocument();
    }

    history_service_->AddPage(
        url, base::Time::Now(),
        history::ContextIDForWebContents(web_contents()),
        web_contents()->GetController().GetLastCommittedEntry()->GetUniqueID(),
        /*referrer=*/GURL(),
        /*redirects=*/{}, ui::PageTransition::PAGE_TRANSITION_TYPED,
        history::VisitSource::SOURCE_BROWSED,
        /*did_replace_entry=*/false);
  }

  BrowsingTopicsPageLoadDataTracker* GetBrowsingTopicsPageLoadDataTracker() {
    return BrowsingTopicsPageLoadDataTracker::GetOrCreateForPage(
        web_contents()->GetPrimaryMainFrame()->GetPage());
  }

  content::BrowsingTopicsSiteDataManager* topics_site_data_manager() {
    return web_contents()
        ->GetBrowserContext()
        ->GetDefaultStoragePartition()
        ->GetBrowsingTopicsSiteDataManager();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<history::HistoryService> history_service_;

  base::ScopedTempDir temp_dir_;
};

TEST_F(BrowsingTopicsPageLoadDataTrackerTest, IniializeWithRedirectStatus) {
  GURL url("https://foo.com");
  NavigateToPage(url, /*publicly_routable=*/true,
                 /*browsing_topics_permissions_policy_allowed=*/true,
                 /*interest_cohort_permissions_policy_allowed=*/true);

  ukm::SourceId source_id = ukm::UkmRecorder::GetNewSourceID();
  BrowsingTopicsPageLoadDataTracker::CreateForPage(
      web_contents()->GetPrimaryMainFrame()->GetPage(),
      /*redirect_count=*/10,
      /*redirect_with_topics_invoked_count=*/5, source_id);

  auto* tracker = GetBrowsingTopicsPageLoadDataTracker();

  EXPECT_EQ(tracker->redirect_count(), 10);
  EXPECT_EQ(tracker->redirect_with_topics_invoked_count(), 5);
  EXPECT_EQ(tracker->source_id_before_redirects(), source_id);
  EXPECT_FALSE(tracker->topics_invoked());

  GetBrowsingTopicsPageLoadDataTracker()->OnBrowsingTopicsApiUsed(
      HashedDomain(123), "bar.com", history_service_.get(), /*observe=*/false);

  EXPECT_TRUE(tracker->topics_invoked());
}

TEST_F(BrowsingTopicsPageLoadDataTrackerTest, OneUsage) {
  GURL url("https://foo.com");
  NavigateToPage(url, /*publicly_routable=*/true,
                 /*browsing_topics_permissions_policy_allowed=*/true,
                 /*interest_cohort_permissions_policy_allowed=*/true);

  EXPECT_FALSE(BrowsingTopicsEligibleForURLVisit(history_service_.get(), url));
  EXPECT_TRUE(
      content::GetBrowsingTopicsApiUsage(topics_site_data_manager()).empty());

  GetBrowsingTopicsPageLoadDataTracker()->OnBrowsingTopicsApiUsed(
      HashedDomain(123), "bar.com", history_service_.get(), /*observe=*/true);

  EXPECT_TRUE(BrowsingTopicsEligibleForURLVisit(history_service_.get(), url));

  std::vector<ApiUsageContext> api_usage_contexts =
      content::GetBrowsingTopicsApiUsage(topics_site_data_manager());
  EXPECT_EQ(api_usage_contexts.size(), 1u);
  EXPECT_EQ(api_usage_contexts[0].hashed_main_frame_host,
            HashMainFrameHostForStorage("foo.com"));
  EXPECT_EQ(api_usage_contexts[0].hashed_context_domain, HashedDomain(123));
}

TEST_F(BrowsingTopicsPageLoadDataTrackerTest, OneUsage_DoesNotObserve) {
  GURL url("https://foo.com");
  NavigateToPage(url, /*publicly_routable=*/true,
                 /*browsing_topics_permissions_policy_allowed=*/true,
                 /*interest_cohort_permissions_policy_allowed=*/true);

  EXPECT_FALSE(BrowsingTopicsEligibleForURLVisit(history_service_.get(), url));
  EXPECT_TRUE(
      content::GetBrowsingTopicsApiUsage(topics_site_data_manager()).empty());

  GetBrowsingTopicsPageLoadDataTracker()->OnBrowsingTopicsApiUsed(
      HashedDomain(123), "bar.com", history_service_.get(), /*observe=*/false);

  EXPECT_FALSE(BrowsingTopicsEligibleForURLVisit(history_service_.get(), url));

  std::vector<ApiUsageContext> api_usage_contexts =
      content::GetBrowsingTopicsApiUsage(topics_site_data_manager());
  EXPECT_EQ(api_usage_contexts.size(), 0u);
}

TEST_F(BrowsingTopicsPageLoadDataTrackerTest, TwoUsages) {
  GURL url("https://foo.com");
  NavigateToPage(url, /*publicly_routable=*/true,
                 /*browsing_topics_permissions_policy_allowed=*/true,
                 /*interest_cohort_permissions_policy_allowed=*/true);

  GetBrowsingTopicsPageLoadDataTracker()->OnBrowsingTopicsApiUsed(
      HashedDomain(123), "bar.com", history_service_.get(), /*observe=*/true);
  GetBrowsingTopicsPageLoadDataTracker()->OnBrowsingTopicsApiUsed(
      HashedDomain(456), "buzz.com", history_service_.get(), /*observe=*/true);

  EXPECT_TRUE(BrowsingTopicsEligibleForURLVisit(history_service_.get(), url));

  std::vector<ApiUsageContext> api_usage_contexts =
      content::GetBrowsingTopicsApiUsage(topics_site_data_manager());
  EXPECT_EQ(api_usage_contexts.size(), 2u);
  EXPECT_EQ(api_usage_contexts[0].hashed_main_frame_host,
            HashMainFrameHostForStorage("foo.com"));
  EXPECT_EQ(api_usage_contexts[0].hashed_context_domain, HashedDomain(123));
  EXPECT_EQ(api_usage_contexts[1].hashed_main_frame_host,
            HashMainFrameHostForStorage("foo.com"));
  EXPECT_EQ(api_usage_contexts[1].hashed_context_domain, HashedDomain(456));
}

TEST_F(BrowsingTopicsPageLoadDataTrackerTest, OneUsage_PageLoadUkm) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  NavigateToPage(GURL("https://foo.com"), /*publicly_routable=*/true,
                 /*browsing_topics_permissions_policy_allowed=*/true,
                 /*interest_cohort_permissions_policy_allowed=*/true);

  GetBrowsingTopicsPageLoadDataTracker()->OnBrowsingTopicsApiUsed(
      HashedDomain(123), "bar.com", history_service_.get(), /*observe=*/true);

  NavigateToPage(GURL(url::kAboutBlankURL), /*publicly_routable=*/true,
                 /*browsing_topics_permissions_policy_allowed=*/true,
                 /*interest_cohort_permissions_policy_allowed=*/true);

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::BrowsingTopics_PageLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());

  ASSERT_EQ(1, ukm::GetExponentialBucketMinForCounts1000(1));

  ukm_recorder.ExpectEntryMetric(entries.back(),
                                 ukm::builders::BrowsingTopics_PageLoad::
                                     kTopicsRequestingContextDomainsCountName,
                                 1);
}

TEST_F(BrowsingTopicsPageLoadDataTrackerTest, OneThousandUsages_PageLoadUkm) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  NavigateToPage(GURL("https://foo.com"), /*publicly_routable=*/true,
                 /*browsing_topics_permissions_policy_allowed=*/true,
                 /*interest_cohort_permissions_policy_allowed=*/true);

  for (int i = 0; i < 1000; ++i) {
    GetBrowsingTopicsPageLoadDataTracker()->OnBrowsingTopicsApiUsed(
        HashedDomain(i), "i.com", history_service_.get(), /*observe=*/true);
  }

  NavigateToPage(GURL(url::kAboutBlankURL), /*publicly_routable=*/true,
                 /*browsing_topics_permissions_policy_allowed=*/true,
                 /*interest_cohort_permissions_policy_allowed=*/true);

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::BrowsingTopics_PageLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());

  ASSERT_EQ(943, ukm::GetExponentialBucketMinForCounts1000(1000));

  ukm_recorder.ExpectEntryMetric(entries.back(),
                                 ukm::builders::BrowsingTopics_PageLoad::
                                     kTopicsRequestingContextDomainsCountName,
                                 943);
}

TEST_F(BrowsingTopicsPageLoadDataTrackerTest, TwoThousandUsages_PageLoadUkm) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  NavigateToPage(GURL("https://foo.com"), /*publicly_routable=*/true,
                 /*browsing_topics_permissions_policy_allowed=*/true,
                 /*interest_cohort_permissions_policy_allowed=*/true);

  for (int i = 0; i < 2000; ++i) {
    GetBrowsingTopicsPageLoadDataTracker()->OnBrowsingTopicsApiUsed(
        HashedDomain(i), "i.com", history_service_.get(), /*observe=*/true);
  }

  NavigateToPage(GURL(url::kAboutBlankURL), /*publicly_routable=*/true,
                 /*browsing_topics_permissions_policy_allowed=*/true,
                 /*interest_cohort_permissions_policy_allowed=*/true);

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::BrowsingTopics_PageLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());

  ASSERT_EQ(943, ukm::GetExponentialBucketMinForCounts1000(1000));
  ASSERT_EQ(1896, ukm::GetExponentialBucketMinForCounts1000(2000));

  ukm_recorder.ExpectEntryMetric(entries.back(),
                                 ukm::builders::BrowsingTopics_PageLoad::
                                     kTopicsRequestingContextDomainsCountName,
                                 943);
}

TEST_F(BrowsingTopicsPageLoadDataTrackerTest, DuplicateDomains) {
  GURL url("https://foo.com");
  NavigateToPage(url, /*publicly_routable=*/true,
                 /*browsing_topics_permissions_policy_allowed=*/true,
                 /*interest_cohort_permissions_policy_allowed=*/true);

  GetBrowsingTopicsPageLoadDataTracker()->OnBrowsingTopicsApiUsed(
      HashedDomain(123), "bar.com", history_service_.get(), /*observe=*/true);
  GetBrowsingTopicsPageLoadDataTracker()->OnBrowsingTopicsApiUsed(
      HashedDomain(456), "buzz.com", history_service_.get(), /*observe=*/true);
  GetBrowsingTopicsPageLoadDataTracker()->OnBrowsingTopicsApiUsed(
      HashedDomain(123), "bar.com", history_service_.get(), /*observe=*/true);

  EXPECT_TRUE(BrowsingTopicsEligibleForURLVisit(history_service_.get(), url));

  std::vector<ApiUsageContext> api_usage_contexts =
      content::GetBrowsingTopicsApiUsage(topics_site_data_manager());
  EXPECT_EQ(api_usage_contexts.size(), 2u);
  EXPECT_EQ(api_usage_contexts[0].hashed_main_frame_host,
            HashMainFrameHostForStorage("foo.com"));
  EXPECT_EQ(api_usage_contexts[0].hashed_context_domain, HashedDomain(123));
  EXPECT_EQ(api_usage_contexts[1].hashed_main_frame_host,
            HashMainFrameHostForStorage("foo.com"));
  EXPECT_EQ(api_usage_contexts[1].hashed_context_domain, HashedDomain(456));

  // The second HashedDomain(123) shouldn't update the database. Verify this by
  // verifying that the timestamp for HashedDomain(123) is no greater than the
  // timestamp for HashedDomain(456).
  EXPECT_LE(api_usage_contexts[0].time, api_usage_contexts[1].time);
}

TEST_F(BrowsingTopicsPageLoadDataTrackerTest, NumberOfDomainsExceedsLimit) {
  GURL url("https://foo.com");
  NavigateToPage(url, /*publicly_routable=*/true,
                 /*browsing_topics_permissions_policy_allowed=*/true,
                 /*interest_cohort_permissions_policy_allowed=*/true);

  for (int i = 0; i < 31; ++i) {
    GetBrowsingTopicsPageLoadDataTracker()->OnBrowsingTopicsApiUsed(
        HashedDomain(i), "i.com", history_service_.get(), /*observe=*/true);
  }

  EXPECT_TRUE(BrowsingTopicsEligibleForURLVisit(history_service_.get(), url));

  std::vector<ApiUsageContext> api_usage_contexts =
      content::GetBrowsingTopicsApiUsage(topics_site_data_manager());

  EXPECT_EQ(api_usage_contexts.size(), 30u);

  for (int i = 0; i < 30; ++i) {
    EXPECT_EQ(api_usage_contexts[i].hashed_main_frame_host,
              HashMainFrameHostForStorage("foo.com"));
    EXPECT_EQ(api_usage_contexts[i].hashed_context_domain, HashedDomain(i));
  }
}

TEST_F(BrowsingTopicsPageLoadDataTrackerTest, NotPubliclyRoutable) {
  GURL url("https://foo.com");
  NavigateToPage(url, /*publicly_routable=*/false,
                 /*browsing_topics_permissions_policy_allowed=*/true,
                 /*interest_cohort_permissions_policy_allowed=*/true);

  GetBrowsingTopicsPageLoadDataTracker()->OnBrowsingTopicsApiUsed(
      HashedDomain(123), "bar.com", history_service_.get(), /*observe=*/true);

  EXPECT_FALSE(BrowsingTopicsEligibleForURLVisit(history_service_.get(), url));
  EXPECT_TRUE(
      content::GetBrowsingTopicsApiUsage(topics_site_data_manager()).empty());
}

TEST_F(BrowsingTopicsPageLoadDataTrackerTest,
       BrowsingTopicsPermissionsPolicyNotAllowed) {
  GURL url("https://foo.com");
  NavigateToPage(url, /*publicly_routable=*/true,
                 /*browsing_topics_permissions_policy_allowed=*/false,
                 /*interest_cohort_permissions_policy_allowed=*/true);

  GetBrowsingTopicsPageLoadDataTracker()->OnBrowsingTopicsApiUsed(
      HashedDomain(123), "bar.com", history_service_.get(), /*observe=*/true);

  EXPECT_FALSE(BrowsingTopicsEligibleForURLVisit(history_service_.get(), url));
  EXPECT_TRUE(
      content::GetBrowsingTopicsApiUsage(topics_site_data_manager()).empty());
}

TEST_F(BrowsingTopicsPageLoadDataTrackerTest,
       InterestCohortPermissionsPolicyNotAllowed) {
  GURL url("https://foo.com");
  NavigateToPage(url, /*publicly_routable=*/true,
                 /*browsing_topics_permissions_policy_allowed=*/true,
                 /*interest_cohort_permissions_policy_allowed=*/false);

  GetBrowsingTopicsPageLoadDataTracker()->OnBrowsingTopicsApiUsed(
      HashedDomain(123), "bar.com", history_service_.get(), /*observe=*/true);

  EXPECT_FALSE(BrowsingTopicsEligibleForURLVisit(history_service_.get(), url));
  EXPECT_TRUE(
      content::GetBrowsingTopicsApiUsage(topics_site_data_manager()).empty());
}

TEST_F(BrowsingTopicsPageLoadDataTrackerTest,
       RendererInitiatedWithUserGesture) {
  GURL url("https://foo.com");
  NavigateToPage(url, /*publicly_routable=*/true,
                 /*browsing_topics_permissions_policy_allowed=*/true,
                 /*interest_cohort_permissions_policy_allowed=*/true,
                 /*browser_initiated=*/false,
                 /*has_user_gesture=*/true);

  GetBrowsingTopicsPageLoadDataTracker()->OnBrowsingTopicsApiUsed(
      HashedDomain(123), "bar.com", history_service_.get(), /*observe=*/true);

  EXPECT_TRUE(BrowsingTopicsEligibleForURLVisit(history_service_.get(), url));

  std::vector<ApiUsageContext> api_usage_contexts =
      content::GetBrowsingTopicsApiUsage(topics_site_data_manager());
  EXPECT_EQ(api_usage_contexts.size(), 1u);
  EXPECT_EQ(api_usage_contexts[0].hashed_main_frame_host,
            HashMainFrameHostForStorage("foo.com"));
  EXPECT_EQ(api_usage_contexts[0].hashed_context_domain, HashedDomain(123));
}

TEST_F(BrowsingTopicsPageLoadDataTrackerTest, RendererInitiatedNoUserGesture) {
  GURL url("https://foo.com");
  NavigateToPage(url, /*publicly_routable=*/true,
                 /*browsing_topics_permissions_policy_allowed=*/true,
                 /*interest_cohort_permissions_policy_allowed=*/true,
                 /*browser_initiated=*/false,
                 /*has_user_gesture=*/false);

  GetBrowsingTopicsPageLoadDataTracker()->OnBrowsingTopicsApiUsed(
      HashedDomain(123), "bar.com", history_service_.get(), /*observe=*/true);

  EXPECT_FALSE(BrowsingTopicsEligibleForURLVisit(history_service_.get(), url));
  EXPECT_TRUE(
      content::GetBrowsingTopicsApiUsage(topics_site_data_manager()).empty());
}

TEST_F(BrowsingTopicsPageLoadDataTrackerTest,
       RendererInitiatedPlusExtraSamePageNav) {
  GURL url("https://foo.com");
  NavigateToPage(url, /*publicly_routable=*/true,
                 /*browsing_topics_permissions_policy_allowed=*/true,
                 /*interest_cohort_permissions_policy_allowed=*/true,
                 /*browser_initiated=*/false,
                 /*has_user_gesture=*/true,
                 /*add_same_document_nav=*/true);

  GetBrowsingTopicsPageLoadDataTracker()->OnBrowsingTopicsApiUsed(
      HashedDomain(123), "bar.com", history_service_.get(), /*observe=*/true);

  EXPECT_TRUE(BrowsingTopicsEligibleForURLVisit(history_service_.get(), url));

  std::vector<ApiUsageContext> api_usage_contexts =
      content::GetBrowsingTopicsApiUsage(topics_site_data_manager());
  EXPECT_EQ(api_usage_contexts.size(), 1u);
  EXPECT_EQ(api_usage_contexts[0].hashed_main_frame_host,
            HashMainFrameHostForStorage("foo.com"));
  EXPECT_EQ(api_usage_contexts[0].hashed_context_domain, HashedDomain(123));
}

}  // namespace browsing_topics
