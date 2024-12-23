// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/content/source_url_recorder.h"
#include "base/test/scoped_feature_list.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/ukm/source.pb.h"
#include "url/gurl.h"

using content::NavigationSimulator;

class SourceUrlRecorderWebContentsObserverTest
    : public content::RenderViewHostTestHarness {
 public:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    ukm::InitializeSourceUrlRecorderForWebContents(web_contents());
  }

  GURL GetAssociatedURLForWebContentsDocument() {
    const ukm::UkmSource* src = test_ukm_recorder_.GetSourceForSourceId(
        web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId());
    return src ? src->url() : GURL();
  }

 protected:
  ukm::TestAutoSetUkmRecorder test_ukm_recorder_;
};

TEST_F(SourceUrlRecorderWebContentsObserverTest, Basic) {
  GURL url("https://www.example.com/");
  NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(), url);

  const auto& sources = test_ukm_recorder_.GetSources();
  // Expect two sources, one for navigation, one for document.
  EXPECT_EQ(2ul, sources.size());
  for (const auto& kv : sources) {
    EXPECT_EQ(url, kv.second->url());
    EXPECT_EQ(1u, kv.second->urls().size());
  }
}

TEST_F(SourceUrlRecorderWebContentsObserverTest, InitialUrl) {
  GURL initial_url("https://www.a.com/");
  GURL final_url("https://www.b.com/");
  auto simulator =
      NavigationSimulator::CreateRendererInitiated(initial_url, main_rfh());
  simulator->Start();
  simulator->Redirect(final_url);
  simulator->Commit();

  EXPECT_EQ(final_url, GetAssociatedURLForWebContentsDocument());

  const auto& sources = test_ukm_recorder_.GetSources();

  // Expect two sources, one for navigation, one for document.
  EXPECT_EQ(2ul, sources.size());
  size_t navigation_type_source_count = 0;
  size_t document_type_source_count = 0;
  for (const auto& kv : sources) {
    if (ukm::GetSourceIdType(kv.first) ==
        ukm::SourceIdObj::Type::NAVIGATION_ID) {
      // The navigation source has both URLs.
      EXPECT_EQ(initial_url, kv.second->urls().front());
      EXPECT_EQ(final_url, kv.second->urls().back());
      navigation_type_source_count++;
    }
    if (ukm::GetSourceIdType(kv.first) == ukm::SourceIdObj::Type::DEFAULT) {
      // The document source has the final URL which is one set on the
      // committed document.
      EXPECT_EQ(final_url, kv.second->urls().front());
      document_type_source_count++;
    }
  }
  EXPECT_EQ(1u, navigation_type_source_count);
  EXPECT_EQ(1u, document_type_source_count);
}

TEST_F(SourceUrlRecorderWebContentsObserverTest, IgnoreUrlInSubframe) {
  GURL main_frame_url("https://www.example.com/");
  GURL sub_frame_url("https://www.example.com/iframe.html");
  NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                    main_frame_url);
  NavigationSimulator::NavigateAndCommitFromDocument(
      sub_frame_url,
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe"));

  const auto& sources = test_ukm_recorder_.GetSources();

  // Expect two sources created, one for navigation and one for document, both
  // should have the URL of the main frame.
  EXPECT_EQ(2ul, sources.size());
  for (const auto& kv : sources) {
    EXPECT_EQ(main_frame_url, kv.second->url());
    EXPECT_EQ(1u, kv.second->urls().size());
  }

  EXPECT_EQ(main_frame_url, GetAssociatedURLForWebContentsDocument());
}

TEST_F(SourceUrlRecorderWebContentsObserverTest, SameDocumentNavigation) {
  GURL url1("https://www.example.com/1");
  GURL url2("https://www.example.com/2");
  GURL same_document_url1("https://www.example.com/#samedocument1");
  GURL same_document_url2("https://www.example.com/#samedocument2");
  NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(), url1);
  NavigationSimulator::CreateRendererInitiated(same_document_url1, main_rfh())
      ->CommitSameDocument();
  NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(), url2);
  NavigationSimulator::CreateRendererInitiated(same_document_url2, main_rfh())
      ->CommitSameDocument();

  EXPECT_EQ(same_document_url2, web_contents()->GetLastCommittedURL());

  // Serialize each source so we can verify expectations below.
  ukm::Source full_nav_source1, full_nav_source2, same_doc_source1,
      same_doc_source2;

  // We expect 6 sources to be created, 4 of which are from the 4 committed
  // navigations, and 2 of which are from the 2 created Document instance.
  EXPECT_EQ(6ul, test_ukm_recorder_.GetSources().size());

  for (auto& kv : test_ukm_recorder_.GetSources()) {
    // Populate protos from the navigation sources.
    if (ukm::GetSourceIdType(kv.first) != ukm::SourceIdObj::Type::NAVIGATION_ID)
      continue;

    if (kv.second->url() == url1) {
      kv.second->PopulateProto(&full_nav_source1);
    } else if (kv.second->url() == url2) {
      kv.second->PopulateProto(&full_nav_source2);
    } else if (kv.second->url() == same_document_url1) {
      kv.second->PopulateProto(&same_doc_source1);
    } else if (kv.second->url() == same_document_url2) {
      kv.second->PopulateProto(&same_doc_source2);
    } else {
      FAIL() << "Encountered unexpected source.";
    }
  }

  // The first navigation was a non-same-document navigation to url1. As such,
  // it shouldn't have any previous source ids.
  EXPECT_EQ(url1, full_nav_source1.urls(0).url());
  EXPECT_TRUE(full_nav_source1.has_id());
  EXPECT_FALSE(full_nav_source1.is_same_document_navigation());
  EXPECT_FALSE(full_nav_source1.has_previous_source_id());
  EXPECT_FALSE(full_nav_source1.has_previous_same_document_source_id());
  EXPECT_TRUE(full_nav_source1.has_navigation_time_msec());

  // The second navigation was a same-document navigation to
  // same_document_url1. It should have a previous_source_id that points to
  // url1's source, but no previous_same_document_source_id.
  EXPECT_EQ(same_document_url1, same_doc_source1.urls(0).url());
  EXPECT_TRUE(same_doc_source1.has_id());
  EXPECT_TRUE(same_doc_source1.is_same_document_navigation());
  EXPECT_EQ(full_nav_source1.id(), same_doc_source1.previous_source_id());
  EXPECT_FALSE(same_doc_source1.has_previous_same_document_source_id());
  EXPECT_TRUE(same_doc_source1.has_navigation_time_msec());

  // The third navigation was a non-same-document navigation to url2. It should
  // have a previous_source_id pointing to the source for url1, and a
  // previous_same_document_source_id pointing to the source for
  // same_document_url1.
  EXPECT_EQ(url2, full_nav_source2.urls(0).url());
  EXPECT_TRUE(full_nav_source2.has_id());
  EXPECT_FALSE(full_nav_source2.is_same_document_navigation());
  EXPECT_EQ(full_nav_source1.id(), full_nav_source2.previous_source_id());
  EXPECT_EQ(same_doc_source1.id(),
            full_nav_source2.previous_same_document_source_id());
  EXPECT_TRUE(full_nav_source2.has_navigation_time_msec());

  // The fourth navigation was a same-document navigation to
  // same_document_url2. It should have a previous_source_id pointing to the
  // source for url2, and no previous_same_document_source_id.
  EXPECT_EQ(same_document_url2, same_doc_source2.urls(0).url());
  EXPECT_TRUE(same_doc_source2.has_id());
  EXPECT_TRUE(same_doc_source2.is_same_document_navigation());
  EXPECT_EQ(full_nav_source2.id(), same_doc_source2.previous_source_id());
  EXPECT_FALSE(same_doc_source2.has_previous_same_document_source_id());
  EXPECT_TRUE(same_doc_source2.has_navigation_time_msec());

  EXPECT_EQ(url2, GetAssociatedURLForWebContentsDocument());

  // The recorded time of each navigation should increase monotonically.
  EXPECT_LE(full_nav_source1.navigation_time_msec(),
            same_doc_source1.navigation_time_msec());
  EXPECT_LE(same_doc_source1.navigation_time_msec(),
            full_nav_source2.navigation_time_msec());
  EXPECT_LE(full_nav_source2.navigation_time_msec(),
            same_doc_source2.navigation_time_msec());
}

TEST_F(SourceUrlRecorderWebContentsObserverTest, NavigationMetadata) {
  GURL url1("https://www.example.com/1");
  GURL same_origin_url1("https://www.example.com/same_origin");
  GURL url2("https://test.example.com/2");
  GURL cross_origin_url2("https://test1.example.com/cross_origin");
  GURL error_url("chrome://error");
  NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(), url1);
  NavigationSimulator::CreateRendererInitiated(same_origin_url1, main_rfh())
      ->Commit();
  NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(), url2);
  NavigationSimulator::CreateRendererInitiated(cross_origin_url2, main_rfh())
      ->Commit();
  NavigationSimulator::NavigateAndFailFromBrowser(web_contents(), error_url,
                                                  net::ERR_FAILED);

  EXPECT_EQ(error_url, web_contents()->GetLastCommittedURL());

  // Serialize each source so we can verify expectations below.
  ukm::Source full_nav_source1;
  ukm::Source full_nav_source2;
  ukm::Source same_origin_source1;
  ukm::Source cross_origin_source2;
  ukm::Source error_page_source;

  for (auto& kv : test_ukm_recorder_.GetSources()) {
    // Populate protos from the navigation sources.
    if (ukm::GetSourceIdType(kv.first) != ukm::SourceIdObj::Type::NAVIGATION_ID)
      continue;

    if (kv.second->url() == url1) {
      kv.second->PopulateProto(&full_nav_source1);
    } else if (kv.second->url() == url2) {
      kv.second->PopulateProto(&full_nav_source2);
    } else if (kv.second->url() == same_origin_url1) {
      kv.second->PopulateProto(&same_origin_source1);
    } else if (kv.second->url() == cross_origin_url2) {
      kv.second->PopulateProto(&cross_origin_source2);
    } else if (kv.second->url() == error_url) {
      kv.second->PopulateProto(&error_page_source);
    } else {
      FAIL() << "Encountered unexpected source.";
    }
  }

  // The first navigation was a browser initiated navigation to url1.
  EXPECT_EQ(url1, full_nav_source1.urls(0).url());
  EXPECT_TRUE(full_nav_source1.has_id());
  EXPECT_FALSE(full_nav_source1.navigation_metadata().is_renderer_initiated());
  EXPECT_EQ(full_nav_source1.navigation_metadata().same_origin_status(),
            ukm::CROSS_ORIGIN);
  EXPECT_FALSE(full_nav_source1.navigation_metadata().is_error_page());

  // The second navigation was a same-origin renderer-initiated navigation to
  // same_origin_url1.
  EXPECT_EQ(same_origin_url1, same_origin_source1.urls(0).url());
  EXPECT_TRUE(same_origin_source1.has_id());
  EXPECT_TRUE(
      same_origin_source1.navigation_metadata().is_renderer_initiated());
  EXPECT_EQ(same_origin_source1.navigation_metadata().same_origin_status(),
            ukm::SAME_ORIGIN);
  EXPECT_FALSE(same_origin_source1.navigation_metadata().is_error_page());

  // The third navigation was a browser initiated navigation to url2.
  EXPECT_EQ(url2, full_nav_source2.urls(0).url());
  EXPECT_TRUE(full_nav_source2.has_id());
  EXPECT_FALSE(full_nav_source2.navigation_metadata().is_renderer_initiated());
  EXPECT_EQ(full_nav_source2.navigation_metadata().same_origin_status(),
            ukm::CROSS_ORIGIN);
  EXPECT_FALSE(full_nav_source2.navigation_metadata().is_error_page());

  // The fourth navigation was a cross-origin renderer-initiated navigation to
  // url2.
  EXPECT_EQ(cross_origin_url2, cross_origin_source2.urls(0).url());
  EXPECT_TRUE(cross_origin_source2.has_id());
  EXPECT_TRUE(
      cross_origin_source2.navigation_metadata().is_renderer_initiated());
  EXPECT_EQ(cross_origin_source2.navigation_metadata().same_origin_status(),
            ukm::CROSS_ORIGIN);
  EXPECT_FALSE(cross_origin_source2.navigation_metadata().is_error_page());

  // The fifth navigation was an error page. Make sure it's is_error_page flag
  // is set.
  EXPECT_EQ(error_url, error_page_source.urls(0).url());
  EXPECT_TRUE(error_page_source.has_id());
  EXPECT_FALSE(error_page_source.navigation_metadata().is_renderer_initiated());
  EXPECT_EQ(error_page_source.navigation_metadata().same_origin_status(),
            ukm::SAME_ORIGIN_STATUS_UNSET);
  EXPECT_TRUE(error_page_source.navigation_metadata().is_error_page());
}
