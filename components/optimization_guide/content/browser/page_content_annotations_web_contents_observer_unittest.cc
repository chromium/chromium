// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_content_annotations_web_contents_observer.h"

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/google/core/common/google_switches.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/test/test_history_database.h"
#include "components/omnibox/browser/fake_autocomplete_provider_client.h"
#include "components/omnibox/browser/zero_suggest_cache_service.h"
#include "components/omnibox/browser/zero_suggest_provider.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/optimization_guide/content/browser/page_content_annotations_service.h"
#include "components/optimization_guide/content/browser/test_optimization_guide_decider.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/page_entities_metadata.pb.h"
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
      history::HistoryService* history_service)
      : PageContentAnnotationsService(nullptr,
                                      "en-US",
                                      optimization_guide_model_provider,
                                      history_service,
                                      nullptr,
                                      nullptr,
                                      nullptr,
                                      base::FilePath(),
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

  void PersistRemotePageMetadata(
      const HistoryVisit& visit,
      const proto::PageEntitiesMetadata& page_metadata) override {
    last_page_metadata_ = page_metadata;
  }

  absl::optional<proto::PageEntitiesMetadata> last_page_metadata_persisted()
      const {
    return last_page_metadata_;
  }

 private:
  absl::optional<HistoryVisit> last_annotation_request_;
  absl::optional<std::pair<HistoryVisit, content::WebContents*>>
      last_related_searches_extraction_request_;
  absl::optional<proto::PageEntitiesMetadata> last_page_metadata_;
};

class FakeOptimizationGuideDecider : public TestOptimizationGuideDecider {
 public:
  void RegisterOptimizationTypes(
      const std::vector<proto::OptimizationType>& optimization_types) override {
    registered_optimization_types_ = optimization_types;
  }

  std::vector<proto::OptimizationType> registered_optimization_types() {
    return registered_optimization_types_;
  }

  void CanApplyOptimizationAsync(
      content::NavigationHandle* navigation_handle,
      proto::OptimizationType optimization_type,
      OptimizationGuideDecisionCallback callback) override {
    DCHECK(optimization_type == proto::PAGE_ENTITIES);

    std::string url_spec = navigation_handle->GetURL().spec();
    if (navigation_handle->GetURL() == GURL("http://hasmetadata.com/")) {
      proto::PageEntitiesMetadata page_entities_metadata;
      page_entities_metadata.set_alternative_title("alternative title");

      OptimizationMetadata metadata;
      metadata.SetAnyMetadataForTesting(page_entities_metadata);
      std::move(callback).Run(OptimizationGuideDecision::kTrue, metadata);
      return;
    }
    if (navigation_handle->GetURL() == GURL("http://wrongmetadata.com/")) {
      OptimizationMetadata metadata;
      proto::Entity entity;
      metadata.SetAnyMetadataForTesting(entity);
      std::move(callback).Run(OptimizationGuideDecision::kTrue, metadata);
      return;
    }
    std::move(callback).Run(OptimizationGuideDecision::kFalse, {});
  }

 private:
  std::vector<proto::OptimizationType> registered_optimization_types_;
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

    optimization_guide_model_provider_ =
        std::make_unique<TestOptimizationGuideModelProvider>();
    history_service_ = std::make_unique<history::HistoryService>();
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    ZeroSuggestProvider::RegisterProfilePrefs(pref_service_->registry());
    zero_suggest_cache_service_ = std::make_unique<ZeroSuggestCacheService>(
        pref_service_.get(), /*cache_size=*/1);
    page_content_annotations_service_ =
        std::make_unique<FakePageContentAnnotationsService>(
            optimization_guide_model_provider_.get(), history_service_.get());

    // Set up a simple template URL service with a default search engine.
    template_url_service_ = std::make_unique<TemplateURLService>(
        kTemplateURLData, std::size(kTemplateURLData));
    template_url_ = template_url_service_->GetTemplateURLForKeyword(
        kDefaultTemplateURLKeyword);
    template_url_service_->SetUserSelectedDefaultSearchProvider(template_url_);

    optimization_guide_decider_ =
        std::make_unique<FakeOptimizationGuideDecider>();

    PageContentAnnotationsWebContentsObserver::CreateForWebContents(
        web_contents(), page_content_annotations_service_.get(),
        template_url_service_.get(), optimization_guide_decider_.get(),
        /*no_state_prefetch_manager=*/nullptr);

    // Overwrite Google base URL.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        ::switches::kGoogleBaseURL, "http://default-engine.com/");
  }

  void TearDown() override {
    page_content_annotations_service_.reset();
    optimization_guide_model_provider_.reset();
    template_url_service_.reset();
    optimization_guide_decider_.reset();

    content::RenderViewHostTestHarness::TearDown();
  }

  FakePageContentAnnotationsService* service() {
    return page_content_annotations_service_.get();
  }

  PageContentAnnotationsWebContentsObserver* helper() {
    return PageContentAnnotationsWebContentsObserver::FromWebContents(
        web_contents());
  }

  FakeOptimizationGuideDecider* optimization_guide_decider() {
    return optimization_guide_decider_.get();
  }

  void SetTemplateURLServiceLoaded(bool loaded) {
    template_url_service_->set_loaded(loaded);
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
  raw_ptr<TemplateURL> template_url_;
  std::unique_ptr<FakeOptimizationGuideDecider> optimization_guide_decider_;
};

TEST_F(PageContentAnnotationsWebContentsObserverTest, DoesNotRegisterType) {
  EXPECT_TRUE(
      optimization_guide_decider()->registered_optimization_types().empty());
}

TEST_F(PageContentAnnotationsWebContentsObserverTest,
       MainFrameNavigationAnnotatesTitle) {
  // Navigate.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://www.foo.com/someurl"));

  // Set title.
  std::u16string title(u"Title");
  web_contents()->UpdateTitleForEntry(controller().GetLastCommittedEntry(),
                                      title);

  // The title should be what is requested to be annotated.
  absl::optional<HistoryVisit> last_annotation_request =
      service()->last_annotation_request();
  EXPECT_TRUE(last_annotation_request.has_value());
  EXPECT_EQ(last_annotation_request->url, GURL("http://www.foo.com/someurl"));
  EXPECT_EQ(last_annotation_request->text_to_annotate, "Title");

  service()->ClearLastAnnotationRequest();

  // Update title again - make sure we don't reannotate for same page.
  web_contents()->UpdateTitleForEntry(controller().GetLastCommittedEntry(),
                                      u"newtitle");
  EXPECT_FALSE(service()->last_annotation_request());
}

TEST_F(PageContentAnnotationsWebContentsObserverTest,
       SameDocumentNavigationsAnnotateTitle) {
  // Navigate.
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("http://foo"), main_rfh());

  // Set title and favicon.
  std::u16string title(u"Title");
  web_contents()->UpdateTitleForEntry(controller().GetLastCommittedEntry(),
                                      title);

  // history.pushState() is called for url2.
  GURL url2("http://foo#foo");
  std::unique_ptr<content::NavigationSimulator> navigation_simulator =
      content::NavigationSimulator::CreateRendererInitiated(url2, main_rfh());
  navigation_simulator->CommitSameDocument();

  // The title should be what is requested to be annotated.
  absl::optional<HistoryVisit> last_annotation_request =
      service()->last_annotation_request();
  EXPECT_TRUE(last_annotation_request.has_value());
  EXPECT_EQ(last_annotation_request->url, url2);
  EXPECT_EQ(last_annotation_request->text_to_annotate, "Title");
}

TEST_F(PageContentAnnotationsWebContentsObserverTest,
       SRPURLsAnnotateSearchTerms) {
  base::HistogramTester histogram_tester;

  // Navigate and commit so there is an entry.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://default-engine.com/search?q=a"));

  // The search query should be what is requested to be annotated.
  absl::optional<HistoryVisit> last_annotation_request =
      service()->last_annotation_request();
  ASSERT_TRUE(last_annotation_request.has_value());
  EXPECT_EQ(last_annotation_request->url,
            GURL("http://default-engine.com/search?q=a"));
  EXPECT_EQ(last_annotation_request->text_to_annotate, "a");

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentAnnotations."
      "TemplateURLServiceLoadedAtNavigationFinish",
      true, 1);
}

TEST_F(PageContentAnnotationsWebContentsObserverTest,
       NonGoogleSRPURLsAnnotateSearchTerms) {
  base::HistogramTester histogram_tester;

  // Navigate and commit so there is an entry.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://non-default-engine.com/?q=a"));

  // The search query should be what is requested to be annotated.
  absl::optional<HistoryVisit> last_annotation_request =
      service()->last_annotation_request();
  ASSERT_TRUE(last_annotation_request.has_value());
  EXPECT_EQ(last_annotation_request->url,
            GURL("http://non-default-engine.com/?q=a"));
  EXPECT_EQ(last_annotation_request->text_to_annotate, "a");

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentAnnotations."
      "TemplateURLServiceLoadedAtNavigationFinish",
      true, 1);
}

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
}

class
    PageContentAnnotationsWebContentsObserverOnlyPersistGoogleSearchMetadataTest
    : public PageContentAnnotationsWebContentsObserverTest {
 public:
  PageContentAnnotationsWebContentsObserverOnlyPersistGoogleSearchMetadataTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kPageContentAnnotations,
        {{"persist_search_metadata_for_non_google_searches", "false"}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(
    PageContentAnnotationsWebContentsObserverOnlyPersistGoogleSearchMetadataTest,
    AnnotatesTitleInsteadOfSearchTerms) {
  // Navigate.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://non-default-engine.com/?q=a"));

  // Set title.
  std::u16string title(u"Title");
  web_contents()->UpdateTitleForEntry(controller().GetLastCommittedEntry(),
                                      title);

  // The title should be what is requested to be annotated.
  absl::optional<HistoryVisit> last_annotation_request =
      service()->last_annotation_request();
  EXPECT_TRUE(last_annotation_request.has_value());
  EXPECT_EQ(last_annotation_request->url,
            GURL("http://non-default-engine.com/?q=a"));
  EXPECT_EQ(last_annotation_request->text_to_annotate, "Title");

  service()->ClearLastAnnotationRequest();

  // Update title again - make sure we don't reannotate for same page.
  web_contents()->UpdateTitleForEntry(controller().GetLastCommittedEntry(),
                                      u"newtitle");
  EXPECT_FALSE(service()->last_annotation_request());
}

TEST_F(
    PageContentAnnotationsWebContentsObserverOnlyPersistGoogleSearchMetadataTest,
    SRPURLsAnnotateTitleIfTemplateURLServiceNotLoaded) {
  SetTemplateURLServiceLoaded(false);

  base::HistogramTester histogram_tester;

  // Navigate and commit so there is an entry.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://default-engine.com/search?q=a"));

  // Set title.
  std::u16string title(u"Title");
  web_contents()->UpdateTitleForEntry(controller().GetLastCommittedEntry(),
                                      title);

  // The title should be what is requested to be annotated.
  absl::optional<HistoryVisit> last_annotation_request =
      service()->last_annotation_request();
  EXPECT_TRUE(last_annotation_request.has_value());
  EXPECT_EQ(last_annotation_request->url,
            GURL("http://default-engine.com/search?q=a"));
  EXPECT_EQ(last_annotation_request->text_to_annotate, "Title");

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentAnnotations."
      "TemplateURLServiceLoadedAtNavigationFinish",
      false, 1);
}

class PageContentAnnotationsWebContentsObserverRemotePageMetadataTest
    : public PageContentAnnotationsWebContentsObserverTest {
 public:
  PageContentAnnotationsWebContentsObserverRemotePageMetadataTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kRemotePageMetadata, {{"persist_page_metadata", "true"}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(PageContentAnnotationsWebContentsObserverRemotePageMetadataTest,
       RegistersTypeWhenFeatureEnabled) {
  std::vector<proto::OptimizationType> registered_optimization_types =
      optimization_guide_decider()->registered_optimization_types();
  EXPECT_EQ(registered_optimization_types.size(), 1u);
  EXPECT_EQ(registered_optimization_types[0], proto::PAGE_ENTITIES);
}

TEST_F(PageContentAnnotationsWebContentsObserverRemotePageMetadataTest,
       DoesNotPersistIfServerHasNoData) {
  // Navigate.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://www.nohints.com/"));

  EXPECT_FALSE(service()->last_page_metadata_persisted());
}

TEST_F(PageContentAnnotationsWebContentsObserverRemotePageMetadataTest,
       DoesNotPersistIfServerReturnsWrongMetadata) {
  // Navigate.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://wrongmetadata.com/"));

  EXPECT_FALSE(service()->last_page_metadata_persisted());
}

TEST_F(PageContentAnnotationsWebContentsObserverRemotePageMetadataTest,
       RequestsToPersistIfHasPageMetadata) {
  // Navigate.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://hasmetadata.com/"));

  absl::optional<proto::PageEntitiesMetadata> metadata =
      service()->last_page_metadata_persisted();
  EXPECT_EQ(metadata->alternative_title(), "alternative title");
}

}  // namespace optimization_guide
