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
#include "components/optimization_guide/content/browser/page_text_dump_result.h"
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

class TestPageTextObserver : public PageTextObserver {
 public:
  explicit TestPageTextObserver(content::WebContents* web_contents)
      : PageTextObserver(web_contents) {}

  void AddConsumer(PageTextObserver::Consumer* consumer) override {
    add_consumer_called_ = true;
  }
  bool add_consumer_called() const { return add_consumer_called_; }

  // We don't test remove consumer since there is no guaranteed ordering when
  // WebContentsObservers are destroyed, so we may hit a segfault.

 private:
  bool add_consumer_called_ = false;
};

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
         {"annotate_title_instead_of_page_content", "false"},
         {"fetch_remote_page_entities", "false"}});
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

    page_text_observer_ = new TestPageTextObserver(web_contents());
    web_contents()->SetUserData(TestPageTextObserver::UserDataKey(),
                                base::WrapUnique(page_text_observer_.get()));

    PageContentAnnotationsWebContentsObserver::CreateForWebContents(
        web_contents(), page_content_annotations_service_.get(),
        template_url_service_.get(), optimization_guide_decider_.get());

    // Overwrite Google base URL.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kGoogleBaseURL, "http://default-engine.com/");
  }

  void TearDown() override {
    page_text_observer_ = nullptr;
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

  TestPageTextObserver* page_text_observer() { return page_text_observer_; }

  FakeOptimizationGuideDecider* optimization_guide_decider() {
    return optimization_guide_decider_.get();
  }

  std::unique_ptr<PageTextObserver::ConsumerTextDumpRequest>
  RequestTextDumpForUrl(const GURL& url, bool is_same_document = false) {
    content::MockNavigationHandle navigation_handle(url, main_rfh());
    navigation_handle.set_url(url);
    // PageTextObserver is guaranteed to call MaybeRequestFrameTextDump after
    // the navigation has been committed.
    navigation_handle.set_has_committed(true);
    navigation_handle.set_is_same_document(is_same_document);
    return helper()->MaybeRequestFrameTextDump(&navigation_handle);
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
  raw_ptr<TestPageTextObserver> page_text_observer_;
  std::unique_ptr<FakeOptimizationGuideDecider> optimization_guide_decider_;
};

TEST_F(PageContentAnnotationsWebContentsObserverTest, DoesNotRegisterType) {
  EXPECT_TRUE(
      optimization_guide_decider()->registered_optimization_types().empty());
}

TEST_F(PageContentAnnotationsWebContentsObserverTest,
       HooksIntoPageTextObserver) {
  EXPECT_TRUE(page_text_observer()->add_consumer_called());
}

TEST_F(PageContentAnnotationsWebContentsObserverTest,
       DoesNotRequestForNonHttpHttps) {
  EXPECT_EQ(RequestTextDumpForUrl(GURL("chrome://new-tab")), nullptr);
}

TEST_F(PageContentAnnotationsWebContentsObserverTest,
       DoesNotRequestForSameDocument) {
  EXPECT_EQ(
      RequestTextDumpForUrl(GURL("http://test.com"), /*is_same_document=*/true),
      nullptr);
}

TEST_F(PageContentAnnotationsWebContentsObserverTest,
       DoesNotRequestForGoogleSRP) {
  EXPECT_EQ(RequestTextDumpForUrl(GURL("http://default-engine.com/search?q=a")),
            nullptr);
}

TEST_F(PageContentAnnotationsWebContentsObserverTest,
       RequestsForMainFrameHttpUrlCallbackDispatchesToService) {
  // Navigate and commit so there is an entry. In actual situations, we are
  // guaranteed that MaybeRequestFrameTextDump will only be called for
  // committed frames.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://test.com"));

  std::unique_ptr<PageTextObserver::ConsumerTextDumpRequest> request =
      RequestTextDumpForUrl(GURL("http://test.com"));
  ASSERT_TRUE(request);
  ASSERT_TRUE(request->callback);
  EXPECT_EQ(features::MaxSizeForPageContentTextDump(), request->max_size);
  EXPECT_TRUE(request->dump_amp_subframes);
  EXPECT_EQ(std::set<mojom::TextDumpEvent>{mojom::TextDumpEvent::kFirstLayout},
            request->events);

  // Invoke OnTextDumpReceived.
  FrameTextDumpResult frame_result =
      FrameTextDumpResult::Initialize(mojom::TextDumpEvent::kFirstLayout,
                                      content::GlobalRenderFrameHostId(),
                                      /*amp_frame=*/false,
                                      /*unique_navigation_id=*/1)
          .CompleteWithContents(u"some text");
  PageTextDumpResult result;
  result.AddFrameTextDumpResult(frame_result);
  std::move(request->callback).Run(std::move(result));

  absl::optional<HistoryVisit> last_annotation_request =
      service()->last_annotation_request();
  EXPECT_TRUE(last_annotation_request.has_value());
  EXPECT_EQ(last_annotation_request->url, GURL("http://test.com"));
  EXPECT_EQ(last_annotation_request->text_to_annotate, "some text");

  service()->ClearLastAnnotationRequest();

  // Update title - make sure we don't annotate if we intend to annotate
  // content.
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

class PageContentAnnotationsWebContentsObserverAnnotateTitleTest
    : public PageContentAnnotationsWebContentsObserverTest {
 public:
  PageContentAnnotationsWebContentsObserverAnnotateTitleTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kPageContentAnnotations,
        {{"annotate_title_instead_of_page_content", "true"}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(PageContentAnnotationsWebContentsObserverAnnotateTitleTest,
       SameDocumentNavigationsStillAnnotatesTitle) {
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

  service()->ClearLastAnnotationRequest();

  // Update title again - make sure we don't reannotate for same page.
  web_contents()->UpdateTitleForEntry(controller().GetLastCommittedEntry(),
                                      u"newtitle");
  EXPECT_FALSE(service()->last_annotation_request());
}

TEST_F(PageContentAnnotationsWebContentsObserverAnnotateTitleTest,
       AnnotatesTitleInsteadOfContent) {
  // Navigate.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://www.foo.com/someurl"));

  // Make sure we didn't register with the PageTextObserver.
  EXPECT_EQ(page_text_observer()->outstanding_requests(), 0u);

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
