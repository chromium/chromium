// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"

#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/preloading/chrome_preloading.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view_observer.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view_test_helper.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/page_load_metrics/browser/navigation_handle_user_data.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/navigation_handle_observer.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/network/public/cpp/features.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/test/test_event.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/test/button_test_api.h"

// Test suite covering the interaction between browser bookmarks and
// `Sec-Fetch-*` headers that can't be covered by Web Platform Tests (yet).
// See https://mikewest.github.io/sec-metadata/#directly-user-initiated and
// https://github.com/web-platform-tests/wpt/issues/16019.
class BookmarkBarNavigationTest : public InProcessBrowserTest,
                                  public content::WebContentsObserver {
 public:
  BookmarkBarNavigationTest()
      : https_test_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  void SetUp() override { InProcessBrowserTest::SetUp(); }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // Setup HTTPS server serving files from standard test directory.
    static constexpr base::FilePath::CharType kDocRoot[] =
        FILE_PATH_LITERAL("chrome/test/data");
    https_test_server_.AddDefaultHandlers(base::FilePath(kDocRoot));
    https_test_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
    ASSERT_TRUE(http_test_server_.Start());
    ASSERT_TRUE(https_test_server_.Start());

    // Setup the mock host resolver
    host_resolver()->AddRule("*", "127.0.0.1");

    browser()->profile()->GetPrefs()->SetBoolean(
        bookmarks::prefs::kShowBookmarkBar, true);

    test_helper_ = std::make_unique<BookmarkBarViewTestHelper>(bookmark_bar());
    Observe(web_contents());
  }

  views::LabelButton* GetBookmarkButton(size_t index) {
    return test_helper_->GetBookmarkButton(index);
  }

  BrowserView* browser_view() {
    return BrowserView::GetBrowserViewForBrowser(browser());
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  BookmarkBarView* bookmark_bar() { return browser_view()->bookmark_bar(); }

  std::string GetContent() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    return content::EvalJs(web_contents, "document.body.textContent")
        .ExtractString();
  }

  void CreateBookmarkForHeader(const std::string& header) {
    // Populate bookmark bar with a single bookmark to
    // `/echoheader?` + |header|.
    bookmarks::BookmarkModel* model =
        BookmarkModelFactory::GetForBrowserContext(browser()->profile());
    bookmarks::test::WaitForBookmarkModelToLoad(model);
    model->DisableWritesToDiskForTest();
    std::string url = "/echoheader?";
    model->AddURL(model->bookmark_bar_node(), 0, u"Example",
                  https_test_server_.GetURL(url + header));
    RunScheduledLayouts();
  }

  void NavigateToBookmark() {
    // Click on the 0th bookmark after setting up a navigation observer that
    // waits for a single navigation to complete successfully.
    content::TestNavigationObserver observer(web_contents(), 1);
    views::LabelButton* button = GetBookmarkButton(0);
    views::test::ButtonTestApi clicker(button);
    ui::test::TestEvent click_event;
    clicker.NotifyClick(click_event);
    observer.Wait();

    // All bookmark navigations should have a null initiator, as there's no
    // web origin from which the navigation is triggered.
    ASSERT_EQ(std::nullopt, observer.last_initiator_origin());
  }

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    auto* navigation_userdata =
        page_load_metrics::NavigationHandleUserData::GetForNavigationHandle(
            *navigation_handle);
    if (navigation_userdata) {
      bookmark_navigation_list_.push_back(
          navigation_userdata->navigation_type());
    }
  }

  const std::vector<
      page_load_metrics::NavigationHandleUserData::InitiatorLocation>&
  bookmark_navigation_list() {
    return bookmark_navigation_list_;
  }

  net::EmbeddedTestServer* https_test_server() { return &https_test_server_; }
  net::EmbeddedTestServer* http_test_server() { return &http_test_server_; }

 private:
  net::EmbeddedTestServer https_test_server_;
  net::EmbeddedTestServer http_test_server_;
  std::unique_ptr<BookmarkBarViewTestHelper> test_helper_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::vector<page_load_metrics::NavigationHandleUserData::InitiatorLocation>
      bookmark_navigation_list_;
};

IN_PROC_BROWSER_TEST_F(BookmarkBarNavigationTest, SecFetchFromEmptyTab) {
  // Navigate to an empty tab
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  {
    // Sec-Fetch-Dest: document
    CreateBookmarkForHeader("Sec-Fetch-Dest");
    NavigateToBookmark();
    EXPECT_EQ("document", GetContent());
  }

  {
    // Sec-Fetch-Mode: navigate
    CreateBookmarkForHeader("Sec-Fetch-Mode");
    NavigateToBookmark();
    EXPECT_EQ("navigate", GetContent());
  }

  {
    // Sec-Fetch-Site: none
    CreateBookmarkForHeader("Sec-Fetch-Site");
    NavigateToBookmark();
    EXPECT_EQ("none", GetContent());
  }

  {
    // Sec-Fetch-User: ?1
    CreateBookmarkForHeader("Sec-Fetch-User");
    NavigateToBookmark();
    EXPECT_EQ("?1", GetContent());
  }
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
//  TODO(crbug.com/40648550): Test flaky on Mac and Windows.
#define MAYBE_SecFetchSiteNoneFromNonEmptyTab \
  DISABLED_SecFetchSiteNoneFromNonEmptyTab
#else
#define MAYBE_SecFetchSiteNoneFromNonEmptyTab SecFetchSiteNoneFromNonEmptyTab
#endif
IN_PROC_BROWSER_TEST_F(BookmarkBarNavigationTest,
                       MAYBE_SecFetchSiteNoneFromNonEmptyTab) {
  // Navigate to an non-empty tab
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("http://example.com/")));

  {
    // Sec-Fetch-Dest: document
    CreateBookmarkForHeader("Sec-Fetch-Dest");
    NavigateToBookmark();
    EXPECT_EQ("document", GetContent());
  }

  {
    // Sec-Fetch-Mode: navigate
    CreateBookmarkForHeader("Sec-Fetch-Mode");
    NavigateToBookmark();
    EXPECT_EQ("navigate", GetContent());
  }

  {
    // Sec-Fetch-Site: none
    CreateBookmarkForHeader("Sec-Fetch-Site");
    NavigateToBookmark();
    EXPECT_EQ("none", GetContent());
  }

  {
    // Sec-Fetch-User: ?1
    CreateBookmarkForHeader("Sec-Fetch-User");
    NavigateToBookmark();
    EXPECT_EQ("?1", GetContent());
  }
}

// Class intercepts invocations of external protocol handlers.
class FakeProtocolHandlerDelegate : public ExternalProtocolHandler::Delegate {
 public:
  FakeProtocolHandlerDelegate() = default;

  // Wait until an external handler url is received by the class method
  // |RunExternalProtocolDialog|.
  const GURL& WaitForUrl() {
    run_loop_.Run();
    return url_invoked_;
  }

  FakeProtocolHandlerDelegate(const FakeProtocolHandlerDelegate& other) =
      delete;
  FakeProtocolHandlerDelegate& operator=(
      const FakeProtocolHandlerDelegate& other) = delete;
  class FakeDefaultSchemeClientWorker
      : public shell_integration::DefaultSchemeClientWorker {
   public:
    explicit FakeDefaultSchemeClientWorker(const GURL& url)
        : DefaultSchemeClientWorker(url) {}
    FakeDefaultSchemeClientWorker(const FakeDefaultSchemeClientWorker& other) =
        delete;
    FakeDefaultSchemeClientWorker& operator=(
        const FakeDefaultSchemeClientWorker& other) = delete;

   private:
    ~FakeDefaultSchemeClientWorker() override = default;
    shell_integration::DefaultWebClientState CheckIsDefaultImpl() override {
      return shell_integration::DefaultWebClientState::NOT_DEFAULT;
    }

    std::u16string GetDefaultClientNameImpl() override { return u"TestApp"; }

    void SetAsDefaultImpl(base::OnceClosure on_finished_callback) override {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(on_finished_callback));
    }
  };

 private:
  scoped_refptr<shell_integration::DefaultSchemeClientWorker> CreateShellWorker(
      const GURL& url) override {
    return base::MakeRefCounted<FakeDefaultSchemeClientWorker>(url);
  }

  void BlockRequest() override { FAIL(); }

  ExternalProtocolHandler::BlockState GetBlockState(const std::string& scheme,
                                                    Profile* profile) override {
    return ExternalProtocolHandler::GetBlockState(scheme, nullptr, profile);
  }

  void RunExternalProtocolDialog(
      const GURL& url,
      content::WebContents* web_contents,
      ui::PageTransition page_transition,
      bool has_user_gesture,
      const std::optional<url::Origin>& initiating_origin,
      const std::u16string& program_name) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    EXPECT_TRUE(url_invoked_.is_empty());
    url_invoked_ = url;
    run_loop_.Quit();
  }

  void LaunchUrlWithoutSecurityCheck(
      const GURL& url,
      content::WebContents* web_contents) override {
    FAIL();
  }

  void FinishedProcessingCheck() override {}

  GURL url_invoked_;
  base::RunLoop run_loop_;
  SEQUENCE_CHECKER(sequence_checker_);
};

// Checks that opening a bookmark to a URL handled by an external handler is not
// blocked by anti-flood protection. Regression test for
// https://crbug.com/1156651
IN_PROC_BROWSER_TEST_F(BookmarkBarNavigationTest, ExternalHandlerAllowed) {
  const char external_protocol[] = "fake";
  const GURL external_url = GURL("fake://path");

  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());
  bookmarks::test::WaitForBookmarkModelToLoad(model);
  model->DisableWritesToDiskForTest();
  model->AddURL(model->bookmark_bar_node(), 0, u"Example", external_url);
  RunScheduledLayouts();
  // First, get into a known (unblocked) state.
  ExternalProtocolHandler::PermitLaunchUrl();
  EXPECT_NE(ExternalProtocolHandler::BLOCK,
            ExternalProtocolHandler::GetBlockState(external_protocol, nullptr,
                                                   browser()->profile()));

  // Next, try to launch a bookmark pointed at the url of an external handler.
  {
    FakeProtocolHandlerDelegate external_handler_delegate;
    ExternalProtocolHandler::SetDelegateForTesting(&external_handler_delegate);
    NavigateToBookmark();
    auto launched_url = external_handler_delegate.WaitForUrl();
    EXPECT_EQ(external_url, launched_url);
    // Verify that the state has returned to block.
    EXPECT_EQ(ExternalProtocolHandler::BLOCK,
              ExternalProtocolHandler::GetBlockState(external_protocol, nullptr,
                                                     browser()->profile()));
  }
  // Finally, without first calling PermitLaunchUrl, try to launch the bookmark.
  {
    FakeProtocolHandlerDelegate external_handler_delegate;
    ExternalProtocolHandler::SetDelegateForTesting(&external_handler_delegate);
    NavigateToBookmark();
    auto launched_url = external_handler_delegate.WaitForUrl();
    EXPECT_EQ(external_url, launched_url);
    // Verify the launch state has changed back.
    EXPECT_EQ(ExternalProtocolHandler::BLOCK,
              ExternalProtocolHandler::GetBlockState(external_protocol, nullptr,
                                                     browser()->profile()));
  }
}

using UkmEntry = ukm::TestUkmRecorder::HumanReadableUkmEntry;
using ukm::builders::Preloading_Attempt;
using ukm::builders::Preloading_Prediction;
static const auto kMockElapsedTime =
    base::ScopedMockElapsedTimersForTest::kMockElapsedTime;

class PrerenderBookmarkBarNavigationTestBase
    : public BookmarkBarNavigationTest {
 public:
  PrerenderBookmarkBarNavigationTestBase()
      : prerender_helper_(base::BindRepeating(
            &PrerenderBookmarkBarNavigationTestBase::GetActiveWebContents,
            base::Unretained(this))) {}

  content::WebContents* GetActiveWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void SetUpOnMainThread() override {
    BookmarkBarNavigationTest::SetUpOnMainThread();
    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
    scoped_test_timer_ =
        std::make_unique<base::ScopedMockElapsedTimersForTest>();
  }

  ukm::TestAutoSetUkmRecorder* test_ukm_recorder() {
    return test_ukm_recorder_.get();
  }

  void CreateBookmarkButton(GURL& prerender_url) {
    // Populate bookmark bar with a single bookmark.
    bookmarks::BookmarkModel* model =
        BookmarkModelFactory::GetForBrowserContext(browser()->profile());
    bookmarks::test::WaitForBookmarkModelToLoad(model);
    model->DisableWritesToDiskForTest();
    model->AddURL(model->bookmark_bar_node(), 0, u"Example", prerender_url);
    RunScheduledLayouts();
  }

  // Currently OnMousePressed will trigger bookmark trigger prerendering,
  // this function simulates the mousePressed and mouseReleased to trigger
  // prerendering and its activation.
  void NavigateToBookmarkByMousePressed(GURL& prerender_url,
                                        bool expect_activation) {
    // Click on the 0th bookmark after setting up a navigation observer that
    // waits for a single navigation to complete successfully.
    content::test::PrerenderHostObserver prerender_observer(
        *GetActiveWebContents(), prerender_url);
    views::LabelButton* button = GetBookmarkButton(0);

    gfx::Point center(10, 10);
    button->OnMouseEntered(ui::MouseEvent(
        ui::EventType::kMouseEntered, center, center, ui::EventTimeForNow(),
        /*flags=*/ui::EF_NONE,
        /*changed_button_flags=*/ui::EF_NONE));
    button->OnMousePressed(ui::MouseEvent(
        ui::EventType::kMousePressed, center, center, ui::EventTimeForNow(),
        ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
    if (expect_activation) {
      content::test::PrerenderTestHelper::WaitForPrerenderLoadCompletion(
          *GetActiveWebContents(), prerender_url);
    }
    button->OnMouseReleased(ui::MouseEvent(
        ui::EventType::kMouseReleased, center, center, ui::EventTimeForNow(),
        ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));

    if (expect_activation) {
      prerender_observer.WaitForActivation();
    } else {
      base::RunLoop().RunUntilIdle();
    }
  }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;
  std::unique_ptr<base::ScopedMockElapsedTimersForTest> scoped_test_timer_;
};

// Following definitions are equal to content::PrerenderFinalStatus.
constexpr int kFinalStatusActivated = 0;
constexpr int kFinalStatusTriggerDestroyed = 16;

constexpr int kPreloadingTriggeringOutcomeSuccess = 5;

class PrerenderBookmarkBarOnPressedNavigationTest
    : public PrerenderBookmarkBarNavigationTestBase {
 public:
  PrerenderBookmarkBarOnPressedNavigationTest() {
    // Mousedown prerender trigger is disabled explicitly and onHover delay is
    // set to 0ms for testing.
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {
            {features::kBookmarkTriggerForPrerender2,
             {{"prerender_bookmarkbar_on_mouse_pressed_trigger", "true"},
              {"prerender_bookmarkbar_on_mouse_hover_trigger", "false"}}},
        },
        /*disabled_features=*/{});
  }

  const content::test::PreloadingAttemptUkmEntryBuilder& ukm_entry_builder() {
    return *ukm_entry_builder_;
  }

  void SetUpOnMainThread() override {
    PrerenderBookmarkBarNavigationTestBase::SetUpOnMainThread();
    ukm_entry_builder_ =
        std::make_unique<content::test::PreloadingAttemptUkmEntryBuilder>(
            chrome_preloading_predictor::kMouseHoverOrMouseDownOnBookmarkBar);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<content::test::PreloadingAttemptUkmEntryBuilder>
      ukm_entry_builder_;
};

// TODO(crbug.com/40285326): This fails with the field trial testing config.
class PrerenderBookmarkBarOnPressedNavigationTestNoTestingConfig
    : public PrerenderBookmarkBarOnPressedNavigationTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    PrerenderBookmarkBarOnPressedNavigationTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch("disable-field-trial-config");
  }
};

IN_PROC_BROWSER_TEST_F(
    PrerenderBookmarkBarOnPressedNavigationTestNoTestingConfig,
    PrerenderActivation) {
  base::HistogramTester histogram_tester;
  // Navigate to an non-empty tab
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_test_server()->GetURL("/empty.html")));
  GURL prerender_url = https_test_server()->GetURL("/empty.html?prerender");

  content::NavigationHandleObserver activation_observer(GetActiveWebContents(),
                                                        prerender_url);

  CreateBookmarkButton(prerender_url);
  NavigateToBookmarkByMousePressed(prerender_url, true);

  EXPECT_EQ(GetActiveWebContents()->GetLastCommittedURL(), prerender_url);

  {
    ukm::SourceId ukm_source_id = activation_observer.next_page_ukm_source_id();
    // Navigate away to flush the metrics and check.
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
    auto ukm_entries = test_ukm_recorder()->GetEntries(
        Preloading_Attempt::kEntryName,
        content::test::kPreloadingAttemptUkmMetrics);
    EXPECT_EQ(ukm_entries.size(), 1u);

    std::vector<UkmEntry> expected_entries = {
        ukm_entry_builder().BuildEntry(
            ukm_source_id, content::PreloadingType::kPrerender,
            content::PreloadingEligibility::kEligible,
            content::PreloadingHoldbackStatus::kAllowed,
            content::PreloadingTriggeringOutcome::kSuccess,
            content::PreloadingFailureReason::kUnspecified,
            /*accurate=*/true,
            /*ready_time=*/kMockElapsedTime),
    };
    EXPECT_THAT(ukm_entries,
                testing::UnorderedElementsAreArray(expected_entries))
        << content::test::ActualVsExpectedUkmEntriesToString(ukm_entries,
                                                             expected_entries);

    auto entries =
        test_ukm_recorder()->GetMergedEntriesByName("PrerenderPageLoad");
    bool witness_bookmarkbar_ukm = false;
    for (auto& kv : entries) {
      const ukm::mojom::UkmEntry* entry = kv.second.get();
      const ukm::UkmSource* source =
          test_ukm_recorder()->GetSourceForSourceId(entry->source_id);
      if (!source) {
        continue;
      }
      EXPECT_TRUE(source->url().is_valid());
      if (source->url() == prerender_url) {
        test_ukm_recorder()->ExpectEntryMetric(
            entry,
            ukm::builders::PrerenderPageLoad::kNavigation_InitiatorLocationName,
            static_cast<int>(page_load_metrics::NavigationHandleUserData::
                                 InitiatorLocation::kBookmarkBar));
        witness_bookmarkbar_ukm = true;
      }
    }
    EXPECT_TRUE(witness_bookmarkbar_ukm);
  }

  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_BookmarkBar",
      kFinalStatusActivated, 1);
  histogram_tester.ExpectUniqueSample(
      "Preloading.Prerender.Attempt.MouseHoverOrMouseDownOnBookmarkBar."
      "TriggeringOutcome",
      kPreloadingTriggeringOutcomeSuccess, 1);
  // Navigate away to flush the metrics and check.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
  ASSERT_EQ(bookmark_navigation_list().size(), 2u);
  for (int i = 0; i < 2; ++i) {
    EXPECT_EQ(bookmark_navigation_list()[i],
              page_load_metrics::NavigationHandleUserData::InitiatorLocation::
                  kBookmarkBar);
  }
  histogram_tester.ExpectTotalCount(
      "Bookmarks.BookmarkBar.PrerenderNavigationToActivation", 1);
}

IN_PROC_BROWSER_TEST_F(PrerenderBookmarkBarOnPressedNavigationTest,
                       PrerenderNonHttps) {
  base::HistogramTester histogram_tester;
  // Navigate to an non-empty tab
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), http_test_server()->GetURL("/empty.html")));
  GURL prerender_url = http_test_server()->GetURL("/empty.html?prerender");
  content::NavigationHandleObserver activation_observer(GetActiveWebContents(),
                                                        prerender_url);

  CreateBookmarkButton(prerender_url);
  NavigateToBookmarkByMousePressed(prerender_url, false);

  {
    ukm::SourceId ukm_source_id = activation_observer.next_page_ukm_source_id();
    // Navigate to a different URL other than the prerender_url to flush the
    // metrics.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), http_test_server()->GetURL("/empty.html")));
    auto attempt_ukm_entries = test_ukm_recorder()->GetEntries(
        Preloading_Attempt::kEntryName,
        content::test::kPreloadingAttemptUkmMetrics);
    EXPECT_EQ(attempt_ukm_entries.size(), 1u);

    UkmEntry expected_entry = ukm_entry_builder().BuildEntry(
        ukm_source_id, content::PreloadingType::kPrerender,
        content::PreloadingEligibility::kHttpsOnly,
        content::PreloadingHoldbackStatus::kUnspecified,
        content::PreloadingTriggeringOutcome::kUnspecified,
        content::PreloadingFailureReason::kUnspecified,
        /*accurate=*/true);
    EXPECT_EQ(attempt_ukm_entries[0], expected_entry)
        << content::test::ActualVsExpectedUkmEntryToString(
               attempt_ukm_entries[0], expected_entry);
  }
}

class PrerenderBookmarkBarOnHoverNavigationTest
    : public PrerenderBookmarkBarNavigationTestBase {
 public:
  PrerenderBookmarkBarOnHoverNavigationTest() {
    // Mousedown prerender trigger is disabled explicitly and onHover delay is
    // set to 0ms for testing.
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {
            {features::kBookmarkTriggerForPrerender2,
             {{"preconnect_start_delay_on_mouse_hover_ms", "0"},
              {"prerender_start_delay_on_mouse_hover_ms", "0"},
              {"prerender_bookmarkbar_on_mouse_pressed_trigger", "false"},
              {"prerender_bookmarkbar_on_mouse_hover_trigger", "true"}}},
        },
        /*disabled_features=*/{});
  }

  const content::test::PreloadingAttemptUkmEntryBuilder& ukm_entry_builder() {
    return *ukm_entry_builder_;
  }

  void SetUpOnMainThread() override {
    PrerenderBookmarkBarNavigationTestBase::SetUpOnMainThread();
    ukm_entry_builder_ =
        std::make_unique<content::test::PreloadingAttemptUkmEntryBuilder>(
            chrome_preloading_predictor::kMouseHoverOrMouseDownOnBookmarkBar);
  }

  void TriggerPrerenderByMouseHoverOnBookmark(bool expect_completion) {
    views::LabelButton* button = GetBookmarkButton(0);

    gfx::Point center(10, 10);
    button->OnMouseEntered(ui::MouseEvent(
        ui::EventType::kMouseEntered, center, center, ui::EventTimeForNow(),
        /*flags=*/ui::EF_NONE,
        /*changed_button_flags=*/ui::EF_NONE));

    if (expect_completion) {
      content::test::PrerenderTestHelper::WaitForPrerenderLoadCompletion(
          *GetActiveWebContents(),
          https_test_server()->GetURL("/empty.html?prerender"));
    } else {
      base::RunLoop().RunUntilIdle();
    }
  }

  void StopPrerenderingByMouseExited() {
    views::LabelButton* button = GetBookmarkButton(0);

    gfx::Point center(10, 10);
    button->OnMouseExited(ui::MouseEvent(ui::EventType::kMouseExited, center,
                                         center, ui::EventTimeForNow(),
                                         /*flags=*/ui::EF_NONE,
                                         /*changed_button_flags=*/ui::EF_NONE));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<content::test::PreloadingAttemptUkmEntryBuilder>
      ukm_entry_builder_;
};

IN_PROC_BROWSER_TEST_F(PrerenderBookmarkBarOnHoverNavigationTest,
                       PrerenderActivation) {
  base::HistogramTester histogram_tester;
  // Navigate to an non-empty tab
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_test_server()->GetURL("/empty.html")));
  GURL prerender_url = https_test_server()->GetURL("/empty.html?prerender");

  content::NavigationHandleObserver activation_observer(GetActiveWebContents(),
                                                        prerender_url);

  CreateBookmarkButton(prerender_url);
  NavigateToBookmarkByMousePressed(prerender_url, true);
  EXPECT_EQ(GetActiveWebContents()->GetLastCommittedURL(),
            https_test_server()->GetURL("/empty.html?prerender"));

  {
    ukm::SourceId ukm_source_id = activation_observer.next_page_ukm_source_id();
    // Navigate away to flush the metrics and check.
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
    auto ukm_entries = test_ukm_recorder()->GetEntries(
        Preloading_Attempt::kEntryName,
        content::test::kPreloadingAttemptUkmMetrics);
    EXPECT_EQ(ukm_entries.size(), 1u);

    std::vector<UkmEntry> expected_entries = {
        ukm_entry_builder().BuildEntry(
            ukm_source_id, content::PreloadingType::kPrerender,
            content::PreloadingEligibility::kEligible,
            content::PreloadingHoldbackStatus::kAllowed,
            content::PreloadingTriggeringOutcome::kSuccess,
            content::PreloadingFailureReason::kUnspecified,
            /*accurate=*/true,
            /*ready_time=*/kMockElapsedTime),
    };
    EXPECT_THAT(ukm_entries,
                testing::UnorderedElementsAreArray(expected_entries))
        << content::test::ActualVsExpectedUkmEntriesToString(ukm_entries,
                                                             expected_entries);
  }

  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_BookmarkBar",
      kFinalStatusActivated, 1);
  histogram_tester.ExpectUniqueSample(
      "Preloading.Prerender.Attempt.MouseHoverOrMouseDownOnBookmarkBar."
      "TriggeringOutcome",
      kPreloadingTriggeringOutcomeSuccess, 1);
  ASSERT_EQ(bookmark_navigation_list().size(), 2u);
  for (int i = 0; i < 2; ++i) {
    EXPECT_EQ(bookmark_navigation_list()[i],
              page_load_metrics::NavigationHandleUserData::InitiatorLocation::
                  kBookmarkBar);
  }
  histogram_tester.ExpectTotalCount(
      "Bookmarks.BookmarkBar.PrerenderNavigationToActivation", 1);
}

// TODO(crbug.com/40285326): This fails with the field trial testing config.
class PrerenderBookmarkBarOnHoverNavigationTestNoTestingConfig
    : public PrerenderBookmarkBarOnHoverNavigationTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    PrerenderBookmarkBarOnHoverNavigationTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch("disable-field-trial-config");
  }
};

// This test verifies prerender cancellation triggered by mouseExited, and
// another prerender can trigger normally after that.
// TODO(crbug.com/40935967): Test times out.
IN_PROC_BROWSER_TEST_F(
    PrerenderBookmarkBarOnHoverNavigationTestNoTestingConfig,
    DISABLED_PrerenderMouseExitedCancellationAndPrerenderActivation) {
  base::HistogramTester histogram_tester;
  // Navigate to an non-empty tab
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_test_server()->GetURL("/empty.html")));
  GURL prerender_url = https_test_server()->GetURL("/empty.html?prerender");

  content::NavigationHandleObserver activation_observer(GetActiveWebContents(),
                                                        prerender_url);

  CreateBookmarkButton(prerender_url);
  // Check mouseExited will cancel the mouseHover prerendering.
  TriggerPrerenderByMouseHoverOnBookmark(true);
  StopPrerenderingByMouseExited();

  EXPECT_EQ(GetActiveWebContents()->GetLastCommittedURL(),
            https_test_server()->GetURL("/empty.html"));
  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_BookmarkBar",
      kFinalStatusTriggerDestroyed, 1);

  // Prerender can trigger and activate normally after previous cancellation.
  NavigateToBookmarkByMousePressed(prerender_url, true);
  EXPECT_EQ(GetActiveWebContents()->GetLastCommittedURL(),
            https_test_server()->GetURL("/empty.html?prerender"));

  {
    ukm::SourceId ukm_source_id = activation_observer.next_page_ukm_source_id();
    // Navigate away to flush the metrics and check.
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
    auto ukm_entries = test_ukm_recorder()->GetEntries(
        Preloading_Attempt::kEntryName,
        content::test::kPreloadingAttemptUkmMetrics);
    EXPECT_EQ(ukm_entries.size(), 2u);

    std::vector<UkmEntry> expected_entries = {
        ukm_entry_builder().BuildEntry(
            ukm_source_id, content::PreloadingType::kPrerender,
            content::PreloadingEligibility::kEligible,
            content::PreloadingHoldbackStatus::kAllowed,
            content::PreloadingTriggeringOutcome::kReady,
            content::PreloadingFailureReason::kUnspecified,
            /*accurate=*/true,
            /*ready_time=*/kMockElapsedTime),
        ukm_entry_builder().BuildEntry(
            ukm_source_id, content::PreloadingType::kPrerender,
            content::PreloadingEligibility::kEligible,
            content::PreloadingHoldbackStatus::kAllowed,
            content::PreloadingTriggeringOutcome::kSuccess,
            content::PreloadingFailureReason::kUnspecified,
            /*accurate=*/true,
            /*ready_time=*/kMockElapsedTime),
    };
    EXPECT_THAT(ukm_entries,
                testing::UnorderedElementsAreArray(expected_entries))
        << content::test::ActualVsExpectedUkmEntriesToString(ukm_entries,
                                                             expected_entries);
  }

  histogram_tester.ExpectBucketCount(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_BookmarkBar",
      kFinalStatusActivated, 1);
  histogram_tester.ExpectBucketCount(
      "Preloading.Prerender.Attempt.MouseHoverOrMouseDownOnBookmarkBar."
      "TriggeringOutcome",
      kPreloadingTriggeringOutcomeSuccess, 1);
}

IN_PROC_BROWSER_TEST_F(PrerenderBookmarkBarOnHoverNavigationTest,
                       SetIsNavigationInDomainCallback) {
  base::HistogramTester histogram_tester;
  // Navigate to an non-empty tab
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_test_server()->GetURL("/empty.html")));
  GURL prerender_url = https_test_server()->GetURL("/empty.html?prerender");

  CreateBookmarkButton(prerender_url);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_test_server()->GetURL("/empty.html?foo")));
  views::LabelButton* button = GetBookmarkButton(0);

  // Mouse enters and exits the button immediately.
  gfx::Point center(10, 10);
  button->OnMouseEntered(ui::MouseEvent(ui::EventType::kMouseEntered, center,
                                        center, ui::EventTimeForNow(),
                                        /*flags=*/ui::EF_NONE,
                                        /*changed_button_flags=*/ui::EF_NONE));
  button->OnMouseExited(ui::MouseEvent(ui::EventType::kMouseExited, center,
                                       center, ui::EventTimeForNow(),
                                       /*flags=*/ui::EF_NONE,
                                       /*changed_button_flags=*/ui::EF_NONE));

  // Normal navigation is not in `kMouseHoverOrMouseDownOnBookmarkBar`
  // predictor's domain.
  histogram_tester.ExpectBucketCount(
      "Preloading.Predictor.MouseHoverOrMouseDownOnBookmarkBar.Recall",
      /*content::PredictorConfusionMatrix::kTruePositive*/ 0, 0);
  histogram_tester.ExpectBucketCount(
      "Preloading.Predictor.MouseHoverOrMouseDownOnBookmarkBar.Recall",
      /*content::PredictorConfusionMatrix::kFalseNegative*/ 3, 0);

  // A `PAGE_TRANSITION_AUTO_BOOKMARK` navigation should be in the predictor's
  // domain.
  CreateBookmarkButton(prerender_url);
  NavigateParams params(browser(),
                        https_test_server()->GetURL("/empty.html?bar"),
                        ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  ui_test_utils::NavigateToURL(&params);
  histogram_tester.ExpectBucketCount(
      "Preloading.Predictor.MouseHoverOrMouseDownOnBookmarkBar.Recall",
      /*content::PredictorConfusionMatrix::kFalseNegative*/ 3, 1);
}

IN_PROC_BROWSER_TEST_F(PrerenderBookmarkBarOnHoverNavigationTest,
                       PrerenderNonHttps) {
  base::HistogramTester histogram_tester;
  // Navigate to an non-empty tab
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_test_server()->GetURL("/empty.html")));
  GURL prerender_url = http_test_server()->GetURL("/empty.html?prerender");

  content::NavigationHandleObserver activation_observer(GetActiveWebContents(),
                                                        prerender_url);

  CreateBookmarkButton(prerender_url);
  // Check mouseExited will cancel the mouseHover prerendering.
  TriggerPrerenderByMouseHoverOnBookmark(false);

  {
    // Navigate away to flush the metrics and check.
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
    ukm::SourceId ukm_source_id =
        GetActiveWebContents()->GetPrimaryMainFrame()->GetPageUkmSourceId();
    auto attempt_ukm_entries = test_ukm_recorder()->GetEntries(
        Preloading_Attempt::kEntryName,
        content::test::kPreloadingAttemptUkmMetrics);
    EXPECT_EQ(attempt_ukm_entries.size(), 1u);

    UkmEntry expected_entry = ukm_entry_builder().BuildEntry(
        ukm_source_id, content::PreloadingType::kPrerender,
        content::PreloadingEligibility::kHttpsOnly,
        content::PreloadingHoldbackStatus::kUnspecified,
        content::PreloadingTriggeringOutcome::kUnspecified,
        content::PreloadingFailureReason::kUnspecified,
        /*accurate=*/false);
    EXPECT_EQ(attempt_ukm_entries[0], expected_entry)
        << content::test::ActualVsExpectedUkmEntryToString(
               attempt_ukm_entries[0], expected_entry);
  }
}

class PrerenderBookmarkBarDisabledNavigationTest
    : public PrerenderBookmarkBarNavigationTestBase {
 public:
  PrerenderBookmarkBarDisabledNavigationTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {
            {features::kBookmarkTriggerForPrerender2,
             {{"prerender_bookmarkbar_on_mouse_pressed_trigger", "false"},
              {"prerender_bookmarkbar_on_mouse_hover_trigger", "false"}}},
        },
        /*disabled_features=*/{});
  }

  const content::test::PreloadingAttemptUkmEntryBuilder& ukm_entry_builder() {
    return *ukm_entry_builder_;
  }

  void SetUpOnMainThread() override {
    PrerenderBookmarkBarNavigationTestBase::SetUpOnMainThread();
    ukm_entry_builder_ =
        std::make_unique<content::test::PreloadingAttemptUkmEntryBuilder>(
            chrome_preloading_predictor::kMouseHoverOrMouseDownOnBookmarkBar);
  }

  void ClickOnBookmarkBarLink() {
    views::LabelButton* button = GetBookmarkButton(0);

    gfx::Point center(10, 10);
    button->OnMousePressed(ui::MouseEvent(
        ui::EventType::kMousePressed, center, center, ui::EventTimeForNow(),
        ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
    button->OnMouseReleased(ui::MouseEvent(
        ui::EventType::kMouseReleased, center, center, ui::EventTimeForNow(),
        ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<content::test::PreloadingAttemptUkmEntryBuilder>
      ukm_entry_builder_;
};

IN_PROC_BROWSER_TEST_F(PrerenderBookmarkBarDisabledNavigationTest,
                       NonPrerenderingBookmarkBarNavigation) {
  base::HistogramTester histogram_tester;
  // Navigate to an non-empty tab
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_test_server()->GetURL("/empty.html")));
  GURL bookmarkbar_url =
      http_test_server()->GetURL("/empty.html?bookmarkbar_url");

  CreateBookmarkButton(bookmarkbar_url);
  ClickOnBookmarkBarLink();

  // Navigate away to flush the metrics and check.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
  ASSERT_EQ(bookmark_navigation_list().size(), 1u);
  for (int i = 0; i < 1; ++i) {
    EXPECT_EQ(bookmark_navigation_list()[i],
              page_load_metrics::NavigationHandleUserData::InitiatorLocation::
                  kBookmarkBar);
  }
  histogram_tester.ExpectTotalCount(
      "Bookmarks.BookmarkBar.PrerenderNavigationToActivation", 0);
}
