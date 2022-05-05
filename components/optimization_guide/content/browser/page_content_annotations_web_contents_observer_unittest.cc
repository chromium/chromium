// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_content_annotations_web_contents_observer.h"

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/google/core/common/google_switches.h"
#include "components/history/core/browser/history_service.h"
#include "components/optimization_guide/content/browser/page_content_annotations_service.h"
#include "components/optimization_guide/content/browser/test_optimization_guide_decider.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/page_entities_metadata.pb.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

namespace {

using ::testing::UnorderedElementsAre;

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
      : PageContentAnnotationsService("en-US",
                                      optimization_guide_model_provider,
                                      history_service,
                                      nullptr,
                                      base::FilePath(),
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

  void PersistRemotePageEntities(
      const HistoryVisit& visit,
      const std::vector<history::VisitContentModelAnnotations::Category>&
          entities) override {
    last_entities_persistence_request_.emplace(std::make_pair(visit, entities));
  }

  absl::optional<
      std::pair<HistoryVisit,
                std::vector<history::VisitContentModelAnnotations::Category>>>
  last_entities_persistence_request() const {
    return last_entities_persistence_request_;
  }

  void PersistSearchMetadata(const HistoryVisit& visit,
                             const SearchMetadata& search_metadata) override {
    last_search_metadata_ = search_metadata;
  }

  absl::optional<SearchMetadata> last_search_metadata_persisted() const {
    return last_search_metadata_;
  }

 private:
  absl::optional<HistoryVisit> last_annotation_request_;
  absl::optional<std::pair<HistoryVisit, content::WebContents*>>
      last_related_searches_extraction_request_;
  absl::optional<
      std::pair<HistoryVisit,
                std::vector<history::VisitContentModelAnnotations::Category>>>
      last_entities_persistence_request_;
  absl::optional<SearchMetadata> last_search_metadata_;
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
    if (navigation_handle->GetURL() == GURL("http://hasentities.com/")) {
      proto::PageEntitiesMetadata page_entities_metadata;
      proto::Entity* entity = page_entities_metadata.add_entities();
      entity->set_entity_id("entity1");
      entity->set_score(50);

      // The following entities should be skipped.
      proto::Entity* entity2 = page_entities_metadata.add_entities();
      entity2->set_score(50);
      proto::Entity* entity3 = page_entities_metadata.add_entities();
      entity3->set_entity_id("scoretoohigh");
      entity3->set_score(105);
      proto::Entity* entity4 = page_entities_metadata.add_entities();
      entity4->set_entity_id("scoretoolow");
      entity4->set_score(-1);

      OptimizationMetadata metadata;
      metadata.SetAnyMetadataForTesting(page_entities_metadata);
      std::move(callback).Run(OptimizationGuideDecision::kTrue, metadata);
      return;
    }
    if (navigation_handle->GetURL() == GURL("http://noentities.com/")) {
      proto::PageEntitiesMetadata page_entities_metadata;
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
        template_url_service_.get(), optimization_guide_decider_.get());

    // Overwrite Google base URL.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kGoogleBaseURL, "http://default-engine.com/");
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

  absl::optional<SearchMetadata> last_search_metadata_persisted =
      service()->last_search_metadata_persisted();
  ASSERT_TRUE(last_search_metadata_persisted.has_value());
  EXPECT_EQ(last_search_metadata_persisted->normalized_url,
            GURL("http://default-engine.com/search?q=a"));
  EXPECT_EQ(last_search_metadata_persisted->search_terms, u"a");

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

  absl::optional<SearchMetadata> last_search_metadata_persisted =
      service()->last_search_metadata_persisted();
  ASSERT_TRUE(last_search_metadata_persisted.has_value());
  EXPECT_EQ(last_search_metadata_persisted->normalized_url,
            GURL("http://non-default-engine.com/?q=a"));
  EXPECT_EQ(last_search_metadata_persisted->search_terms, u"a");

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

  // Search metadata should not be persisted.
  absl::optional<SearchMetadata> last_search_metadata_persisted =
      service()->last_search_metadata_persisted();
  ASSERT_FALSE(last_search_metadata_persisted.has_value());
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

  // We don't know what the search terms are so no search metadata is persisted.
  absl::optional<SearchMetadata> last_search_metadata_persisted =
      service()->last_search_metadata_persisted();
  ASSERT_FALSE(last_search_metadata_persisted.has_value());

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

class PageContentAnnotationsWebContentsObserverRemotePageEntitiesTest
    : public PageContentAnnotationsWebContentsObserverTest {
 public:
  PageContentAnnotationsWebContentsObserverRemotePageEntitiesTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kPageContentAnnotations,
        {{"fetch_remote_page_entities", "true"}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(PageContentAnnotationsWebContentsObserverRemotePageEntitiesTest,
       RegistersTypeWhenFeatureEnabled) {
  std::vector<proto::OptimizationType> registered_optimization_types =
      optimization_guide_decider()->registered_optimization_types();
  EXPECT_EQ(registered_optimization_types.size(), 1u);
  EXPECT_EQ(registered_optimization_types[0], proto::PAGE_ENTITIES);
}

TEST_F(PageContentAnnotationsWebContentsObserverRemotePageEntitiesTest,
       DoesNotPersistIfServerHasNoData) {
  // Navigate.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://www.nohints.com/"));

  EXPECT_FALSE(service()->last_entities_persistence_request());
}

TEST_F(PageContentAnnotationsWebContentsObserverRemotePageEntitiesTest,
       DoesNotPersistIfNoEntities) {
  // Navigate.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://noentities.com/"));

  EXPECT_FALSE(service()->last_entities_persistence_request());
}

TEST_F(PageContentAnnotationsWebContentsObserverRemotePageEntitiesTest,
       DoesNotPersistIfServerReturnsWrongMetadata) {
  // Navigate.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://wrongmetadata.com/"));

  EXPECT_FALSE(service()->last_entities_persistence_request());
}

TEST_F(PageContentAnnotationsWebContentsObserverRemotePageEntitiesTest,
       RequestsToPersistIfHasEntities) {
  // Navigate.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://hasentities.com/"));

  absl::optional<
      std::pair<HistoryVisit,
                std::vector<history::VisitContentModelAnnotations::Category>>>
      request = service()->last_entities_persistence_request();
  ASSERT_TRUE(request);
  EXPECT_EQ(request->first.url, GURL("http://hasentities.com/"));
  EXPECT_THAT(
      request->second,
      UnorderedElementsAre(
          history::VisitContentModelAnnotations::Category("entity1", 50)));
}

}  // namespace optimization_guide
