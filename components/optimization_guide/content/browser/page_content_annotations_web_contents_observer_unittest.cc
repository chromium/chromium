// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_content_annotations_web_contents_observer.h"

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/google/core/common/google_switches.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/test/test_history_database.h"
#include "components/omnibox/browser/fake_autocomplete_provider_client.h"
#include "components/omnibox/browser/zero_suggest_cache_service.h"
#include "components/omnibox/browser/zero_suggest_provider.h"
#include "components/optimization_guide/content/browser/page_content_annotations_service.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "components/prefs/testing_pref_service.h"
#include "components/search_engines/template_url_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/test/fake_local_frame.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/metrics/public/mojom/ukm_interface.mojom-forward.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/frame/frame.mojom.h"
#include "third_party/blink/public/mojom/opengraph/metadata.mojom.h"
#include "url/gurl.h"

namespace optimization_guide {

namespace {

using ::testing::UnorderedElementsAre;

class FrameRemoteTester : public content::FakeLocalFrame {
 public:
  FrameRemoteTester() = default;
  ~FrameRemoteTester() override = default;

  bool did_get_request() const { return did_get_request_; }

  void set_open_graph_md_response(blink::mojom::OpenGraphMetadataPtr response) {
    response_ = std::move(response);
  }

  void BindPendingReceiver(mojo::ScopedInterfaceEndpointHandle handle) {
    receivers_.Add(this,
                   mojo::PendingAssociatedReceiver<blink::mojom::LocalFrame>(
                       std::move(handle)));
  }

  // blink::mojom::LocalFrame:
  void GetOpenGraphMetadata(
      base::OnceCallback<void(blink::mojom::OpenGraphMetadataPtr)> callback)
      override {
    did_get_request_ = true;
    std::move(callback).Run(std::move(response_));
  }

 private:
  mojo::AssociatedReceiverSet<blink::mojom::LocalFrame> receivers_;
  bool did_get_request_ = false;
  blink::mojom::OpenGraphMetadataPtr response_;
};

continuous_search::mojom::CategoryResultsPtr
GenerateMockRelatedSearchExtractorResults(
    const GURL& url,
    const std::vector<std::string>& mock_related_searches) {
  continuous_search::mojom::CategoryResultsPtr results =
      continuous_search::mojom::CategoryResults::New();
  results->document_url = url;
  results->category_type = continuous_search::mojom::Category::kOrganic;
  {
    continuous_search::mojom::ResultGroupPtr result_group =
        continuous_search::mojom::ResultGroup::New();
    result_group->type = continuous_search::mojom::ResultType::kRelatedSearches;
    for (const auto& search : mock_related_searches) {
      continuous_search::mojom::SearchResultPtr result =
          continuous_search::mojom::SearchResult::New();
      result->title = base::UTF8ToUTF16(search);
      result_group->results.push_back(std::move(result));
    }
    results->groups.push_back(std::move(result_group));
  }

  return results;
}

}  // namespace

const TemplateURLService::Initializer kTemplateURLData[] = {
    {"default-engine.com", "http://default-engine.com/search?q={searchTerms}",
     "Default"},
    {"non-default-engine.com", "http://non-default-engine.com?q={searchTerms}",
     "Not Default"},
};
const char16_t kDefaultTemplateURLKeyword[] = u"default-engine.com";

class FakePageContentAnnotationsService : public PageContentAnnotationsService {
 public:
  explicit FakePageContentAnnotationsService(
      OptimizationGuideModelProvider* optimization_guide_model_provider,
      history::HistoryService* history_service,
      ZeroSuggestCacheService* zero_suggest_cache_service,
      TemplateURLService* template_url_service)
      : PageContentAnnotationsService(
            std::make_unique<FakeAutocompleteProviderClient>(),
            "en-US",
            "us",
            optimization_guide_model_provider,
            history_service,
            template_url_service,
            zero_suggest_cache_service,
            nullptr,
            base::FilePath(),
            nullptr,
            nullptr,
            nullptr) {}
  ~FakePageContentAnnotationsService() override = default;

  void Annotate(const HistoryVisit& visit) override {
    last_annotation_request_.emplace(visit);
  }

  void ExtractRelatedSearches(const HistoryVisit& visit,
                              content::WebContents* web_contents) override {
    last_related_searches_extraction_request_.emplace(
        std::make_pair(visit, web_contents));

    OnRelatedSearchesExtracted(
        visit, continuous_search::SearchResultExtractorClientStatus::kSuccess,
        GenerateMockRelatedSearchExtractorResults(visit.url,
                                                  {"mountain view"}));
  }

  void AddRelatedSearchesForVisit(
      const HistoryVisit& visit,
      const std::vector<std::string>& related_searches) override {
    last_related_searches_extraction_results_.emplace(related_searches);
  }

  absl::optional<HistoryVisit> last_annotation_request() const {
    return last_annotation_request_;
  }

  void ClearLastAnnotationRequest() {
    last_annotation_request_ = absl::nullopt;
  }

  absl::optional<std::pair<HistoryVisit, content::WebContents*>>
  last_related_searches_extraction_request() const {
    return last_related_searches_extraction_request_;
  }

  absl::optional<std::vector<std::string>>
  last_related_searches_extraction_results() const {
    return last_related_searches_extraction_results_;
  }

 private:
  absl::optional<HistoryVisit> last_annotation_request_;
  absl::optional<std::pair<HistoryVisit, content::WebContents*>>
      last_related_searches_extraction_request_;
  absl::optional<std::vector<std::string>>
      last_related_searches_extraction_results_;
};

class PageContentAnnotationsWebContentsObserverTest
    : public content::RenderViewHostTestHarness {
 public:
  PageContentAnnotationsWebContentsObserverTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kPageContentAnnotations,
        {{"extract_related_searches", "false"},
         {"fetch_remote_page_entities", "false"},
         {"persist_search_metadata_for_non_google_searches", "true"}});
  }

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    history_service_ = std::make_unique<history::HistoryService>();
    ASSERT_TRUE(history_service_->Init(
        history::TestHistoryDatabaseParamsForPath(temp_dir_.GetPath())));

    optimization_guide_model_provider_ =
        std::make_unique<TestOptimizationGuideModelProvider>();
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    ZeroSuggestProvider::RegisterProfilePrefs(pref_service_->registry());
    zero_suggest_cache_service_ = std::make_unique<ZeroSuggestCacheService>(
        pref_service_.get(), /*cache_size=*/1);

    // Set up a simple template URL service with a default search engine.
    template_url_service_ = std::make_unique<TemplateURLService>(
        kTemplateURLData, std::size(kTemplateURLData));
    template_url_ = template_url_service_->GetTemplateURLForKeyword(
        kDefaultTemplateURLKeyword);
    template_url_service_->SetUserSelectedDefaultSearchProvider(template_url_);

    page_content_annotations_service_ =
        std::make_unique<FakePageContentAnnotationsService>(
            optimization_guide_model_provider_.get(), history_service_.get(),
            zero_suggest_cache_service_.get(), template_url_service_.get());

    PageContentAnnotationsWebContentsObserver::CreateForWebContents(
        web_contents(), page_content_annotations_service_.get(),
        template_url_service_.get(), /*no_state_prefetch_manager=*/nullptr);

    // Overwrite Google base URL.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        ::switches::kGoogleBaseURL, "http://default-engine.com/");
  }

  void TearDown() override {
    history_service_->Shutdown();
    task_environment()->RunUntilIdle();

    page_content_annotations_service_.reset();
    optimization_guide_model_provider_.reset();
    template_url_service_.reset();

    content::RenderViewHostTestHarness::TearDown();
  }

  FakePageContentAnnotationsService* service() {
    return page_content_annotations_service_.get();
  }

  history::HistoryService* history_service() { return history_service_.get(); }

  ZeroSuggestCacheService* zero_suggest_cache_service() {
    return zero_suggest_cache_service_.get();
  }

  PageContentAnnotationsWebContentsObserver* helper() {
    return PageContentAnnotationsWebContentsObserver::FromWebContents(
        web_contents());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestOptimizationGuideModelProvider>
      optimization_guide_model_provider_;
  std::unique_ptr<history::HistoryService> history_service_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  std::unique_ptr<ZeroSuggestCacheService> zero_suggest_cache_service_;
  std::unique_ptr<FakePageContentAnnotationsService>
      page_content_annotations_service_;
  std::unique_ptr<TemplateURLService> template_url_service_;
  raw_ptr<TemplateURL, DanglingUntriaged> template_url_;
};

TEST_F(PageContentAnnotationsWebContentsObserverTest,
       RequestsRelatedSearchesForMainFrameSRPUrl) {
  // Navigate to non-Google SRP and commit.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://www.foo.com/search?q=a"));

  absl::optional<std::pair<HistoryVisit, content::WebContents*>> last_request =
      service()->last_related_searches_extraction_request();
  EXPECT_FALSE(last_request.has_value());

  // Navigate to Google SRP and commit.
  // No request should be sent since extracting related searches is disabled.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://default-engine.com/search?q=a"));
  last_request = service()->last_related_searches_extraction_request();
  EXPECT_FALSE(last_request.has_value());
}

TEST_F(PageContentAnnotationsWebContentsObserverTest, OgImagePresent) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  auto metadata = blink::mojom::OpenGraphMetadata::New();
  metadata->image = GURL("http://www.google.com/image.png");
  FrameRemoteTester frame_remote_tester;
  frame_remote_tester.set_open_graph_md_response(std::move(metadata));

  main_rfh()->GetRemoteAssociatedInterfaces()->OverrideBinderForTesting(
      blink::mojom::LocalFrame::Name_,
      base::BindRepeating(&FrameRemoteTester::BindPendingReceiver,
                          base::Unretained(&frame_remote_tester)));

  auto nav_simulator = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("http://foo.com/bar"), web_contents());
  nav_simulator->Commit();
  nav_simulator->StopLoading();
  task_environment()->RunUntilIdle();

  ASSERT_TRUE(frame_remote_tester.did_get_request());

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentAnnotations.SalientImageAvailability",
      /*kAvailableFromOgImage=*/3, 1);

  std::vector<const ukm::mojom::UkmEntry*> entries =
      ukm_recorder.GetEntriesByName(
          ukm::builders::SalientImageAvailability::kEntryName);
  ASSERT_EQ(1u, entries.size());

  ASSERT_EQ(1u, entries[0]->metrics.size());
  EXPECT_EQ(/*kAvailableFromOgImage=*/3, entries[0]->metrics.begin()->second);
}

TEST_F(PageContentAnnotationsWebContentsObserverTest, OgImageMalformed) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  auto metadata = blink::mojom::OpenGraphMetadata::New();
  metadata->image = GURL();
  FrameRemoteTester frame_remote_tester;
  frame_remote_tester.set_open_graph_md_response(std::move(metadata));

  main_rfh()->GetRemoteAssociatedInterfaces()->OverrideBinderForTesting(
      blink::mojom::LocalFrame::Name_,
      base::BindRepeating(&FrameRemoteTester::BindPendingReceiver,
                          base::Unretained(&frame_remote_tester)));

  auto nav_simulator = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("http://foo.com/bar"), web_contents());
  nav_simulator->Commit();
  nav_simulator->StopLoading();
  task_environment()->RunUntilIdle();

  ASSERT_TRUE(frame_remote_tester.did_get_request());

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentAnnotations.SalientImageAvailability",
      /*kNotAvailable=*/1, 1);

  std::vector<const ukm::mojom::UkmEntry*> entries =
      ukm_recorder.GetEntriesByName(
          ukm::builders::SalientImageAvailability::kEntryName);
  ASSERT_EQ(1u, entries.size());

  // Malformed URL is also reported as og image unavailable.
  ASSERT_EQ(1u, entries[0]->metrics.size());
  EXPECT_EQ(/*kNotAvailable=*/1, entries[0]->metrics.begin()->second);
}

TEST_F(PageContentAnnotationsWebContentsObserverTest, NoOgImage) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  // Image not set on |metadata|.
  auto metadata = blink::mojom::OpenGraphMetadata::New();
  FrameRemoteTester frame_remote_tester;
  frame_remote_tester.set_open_graph_md_response(std::move(metadata));

  main_rfh()->GetRemoteAssociatedInterfaces()->OverrideBinderForTesting(
      blink::mojom::LocalFrame::Name_,
      base::BindRepeating(&FrameRemoteTester::BindPendingReceiver,
                          base::Unretained(&frame_remote_tester)));

  auto nav_simulator = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("http://foo.com/bar"), web_contents());
  nav_simulator->Commit();
  nav_simulator->StopLoading();
  task_environment()->RunUntilIdle();

  ASSERT_TRUE(frame_remote_tester.did_get_request());

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentAnnotations.SalientImageAvailability",
      /*kNotAvailable=*/1, 1);

  std::vector<const ukm::mojom::UkmEntry*> entries =
      ukm_recorder.GetEntriesByName(
          ukm::builders::SalientImageAvailability::kEntryName);
  ASSERT_EQ(1u, entries.size());

  ASSERT_EQ(1u, entries[0]->metrics.size());
  EXPECT_EQ(/*kNotAvailable=*/1, entries[0]->metrics.begin()->second);
}

TEST_F(PageContentAnnotationsWebContentsObserverTest, OgImageIsNotHTTP) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  auto metadata = blink::mojom::OpenGraphMetadata::New();
  metadata->image = GURL("ftp://foo.com");
  FrameRemoteTester frame_remote_tester;
  frame_remote_tester.set_open_graph_md_response(std::move(metadata));

  main_rfh()->GetRemoteAssociatedInterfaces()->OverrideBinderForTesting(
      blink::mojom::LocalFrame::Name_,
      base::BindRepeating(&FrameRemoteTester::BindPendingReceiver,
                          base::Unretained(&frame_remote_tester)));

  auto nav_simulator = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("http://foo.com/bar"), web_contents());
  nav_simulator->Commit();
  nav_simulator->StopLoading();
  task_environment()->RunUntilIdle();

  ASSERT_TRUE(frame_remote_tester.did_get_request());

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentAnnotations.SalientImageAvailability",
      /*kNotAvailable=*/1, 1);

  std::vector<const ukm::mojom::UkmEntry*> entries =
      ukm_recorder.GetEntriesByName(
          ukm::builders::SalientImageAvailability::kEntryName);
  ASSERT_EQ(1u, entries.size());

  // Non-HTTP URL is also reported as og image unavailable.
  ASSERT_EQ(1u, entries[0]->metrics.size());
  EXPECT_EQ(/*kNotAvailable=*/1, entries[0]->metrics.begin()->second);
}

class PageContentAnnotationsWebContentsObserverRelatedSearchesTest
    : public PageContentAnnotationsWebContentsObserverTest {
 public:
  PageContentAnnotationsWebContentsObserverRelatedSearchesTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kPageContentAnnotations,
        {{"extract_related_searches", "true"}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(PageContentAnnotationsWebContentsObserverRelatedSearchesTest,
       RequestsRelatedSearchesForMainFrameSRPUrl) {
  // Navigate to non-Google SRP and commit.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://www.foo.com/search?q=a"));

  absl::optional<std::pair<HistoryVisit, content::WebContents*>> last_request =
      service()->last_related_searches_extraction_request();
  EXPECT_FALSE(last_request.has_value());

  // Navigate to Google SRP and commit.
  // Expect a request to be sent since extracting related searches is enabled.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://default-engine.com/search?q=a"));
  last_request = service()->last_related_searches_extraction_request();
  EXPECT_TRUE(last_request.has_value());
  EXPECT_EQ(last_request->first.url,
            GURL("http://default-engine.com/search?q=a"));
  EXPECT_EQ(last_request->second, web_contents());

  auto last_results = service()->last_related_searches_extraction_results();
  EXPECT_TRUE(last_results.has_value());

  auto related_searches = last_results.value();
  EXPECT_FALSE(related_searches.empty());
  EXPECT_EQ(related_searches[0], "mountain view");
}

class PageContentAnnotationsWebContentsObserverRelatedSearchesFromZPSCacheTest
    : public PageContentAnnotationsWebContentsObserverTest {
 public:
  PageContentAnnotationsWebContentsObserverRelatedSearchesFromZPSCacheTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{features::kPageContentAnnotations,
          {{"extract_related_searches", "true"}}},
         {features::kExtractRelatedSearchesFromPrefetchedZPSResponse, {}}},
        /*disabled_features=*/{});
  }

  void StoreMockZeroSuggestResponse(
      ZeroSuggestCacheService* zero_suggest_cache_service,
      const std::string& page_url,
      const std::string& response_json) {
    DCHECK(zero_suggest_cache_service);
    zero_suggest_cache_service->StoreZeroSuggestResponse(page_url,
                                                         response_json);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(PageContentAnnotationsWebContentsObserverRelatedSearchesFromZPSCacheTest,
       ExtractRelatedSearchesFromCacheForMainFrameSRPUrl) {
  std::string response_json = R"([
    "",
    ["los angeles", "san diego", "san francisco"],
    ["", "", ""],
    [],
    {
      "google:clientdata": {
        "bpc": false,
        "tlw": false
      },
      "google:suggestdetail": [{}, {}, {}],
      "google:suggestrelevance": [701, 700, 553],
      "google:suggestsubtypes": [
        [512, 433, 67],
        [131, 433, 67],
        [512, 433, 67]
      ],
      "google:suggesttype": ["QUERY", "ENTITY", "ENTITY"],
      "google:verbatimrelevance": 851
    }])";

  // Verify proper behavior when navigating to non-Google SRP.
  {
    const GURL non_google_srp_url = GURL("http://www.foo.com/search?q=a+b+c");

    // Trigger ZPS prefetching on non-Google SRP.
    StoreMockZeroSuggestResponse(zero_suggest_cache_service(),
                                 non_google_srp_url.spec(), response_json);

    // Navigate to non-Google SRP and commit.
    content::NavigationSimulator::NavigateAndCommitFromBrowser(
        web_contents(), non_google_srp_url);

    // Add non-Google SRP visit to history DB.
    history_service()->AddPage(non_google_srp_url, base::Time::Now(),
                               history::VisitSource::SOURCE_BROWSED);
    task_environment()->RunUntilIdle();

    // Extractor request will NOT be sent for a non-Google SRP visit.
    auto last_request = service()->last_related_searches_extraction_request();
    EXPECT_FALSE(last_request.has_value());

    // No "related searches" extractor results should be present.
    auto last_results = service()->last_related_searches_extraction_results();
    EXPECT_FALSE(last_results.has_value());
  }

  // Verify proper behavior when navigating to Google SRP.
  {
    const GURL google_srp_url =
        GURL("http://default-engine.com/search?q=a+b+c");

    // Trigger ZPS prefetching on Google SRP.
    StoreMockZeroSuggestResponse(zero_suggest_cache_service(),
                                 google_srp_url.spec(), response_json);

    // Navigate to Google SRP and commit.
    content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                               google_srp_url);

    // Add Google SRP visit to history DB.
    history_service()->AddPage(google_srp_url, base::Time::Now(),
                               history::VisitSource::SOURCE_BROWSED);
    task_environment()->RunUntilIdle();

    // Extractor request will be sent for a Google SRP visit.
    auto last_request = service()->last_related_searches_extraction_request();
    EXPECT_TRUE(last_request.has_value());
    EXPECT_EQ(last_request->first.url, google_srp_url);
    EXPECT_EQ(last_request->second, web_contents());

    auto last_results = service()->last_related_searches_extraction_results();
    EXPECT_TRUE(last_results.has_value());

    auto related_searches = last_results.value();
    EXPECT_FALSE(related_searches.empty());

    // The full set of "related searches" for this visit should be a combination
    // of those obtained via SRP DOM extraction and those sourced from ZPS
    // prefetching on SRP.
    EXPECT_EQ(related_searches[0], "mountain view");
    EXPECT_EQ(related_searches[1], "los angeles");
    EXPECT_EQ(related_searches[2], "san diego");
    EXPECT_EQ(related_searches[3], "san francisco");
  }
}

}  // namespace optimization_guide
