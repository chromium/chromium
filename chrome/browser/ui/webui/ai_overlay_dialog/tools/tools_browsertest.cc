// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ai_overlay_dialog/tools/tools.h"

#include <memory>
#include <string>

#include "base/hash/hash.h"
#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/translate/translate_test_utils.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/ai_overlay_dialog/ai_overlay_dialog_page_handler.h"
#include "chrome/browser/ui/webui/ai_overlay_dialog/page_context_monitor.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_interface.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/common/translate_switches.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/dom/dom_node_id.h"
#include "ui/base/base_window.h"
#include "ui/base/window_open_disposition.h"

namespace ttc {
namespace {

using PerformSearchResult = base::expected<std::monostate, std::string>;
using FindAndHighlightResult = base::expected<std::monostate, std::string>;
using PlayVideoResult = base::expected<std::monostate, std::string>;
using PauseVideoResult = base::expected<std::monostate, std::string>;
using SeekToTimestampResult = base::expected<std::monostate, std::string>;
using SwitchTabResult =
    base::expected<ai_overlay_dialog::mojom::SwitchTabResultPtr, std::string>;
using CloseTabResult = base::expected<std::monostate, std::string>;
using GoBackResult = base::expected<std::monostate, std::string>;
using GoForwardResult = base::expected<std::monostate, std::string>;
using ReloadResult = base::expected<std::monostate, std::string>;
using TranslatePageResult = base::expected<std::monostate, std::string>;
using FollowLinkResult = base::expected<std::monostate, std::string>;

class FakePage : public ai_overlay_dialog::mojom::Page {
 public:
  FakePage() = default;
  ~FakePage() override = default;

  mojo::PendingRemote<ai_overlay_dialog::mojom::Page> BindAndGetRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  // ai_overlay_dialog::mojom::Page:
  void DidChangePage(const std::string& url,
                     const std::optional<std::string>& title,
                     const std::optional<std::string>& content) override {}
  void UpdateCurrentPageContext(
      const std::string& page_title,
      ai_overlay_dialog::mojom::PageContentNodePtr root_node,
      std::vector<ai_overlay_dialog::mojom::WebMcpToolDefinitionPtr>
          webmcp_tools) override {}
  void SetInputCaptionsVisible(bool visible) override {}
  void SetOutputCaptionsVisible(bool visible) override {}
  void SetUsePersona(bool use_persona) override {}
  void OnStreamingSessionStateChanged(
      bool connected,
      const std::string& session_id,
      const std::string& error_message) override {}
  void OnTranscriptions(const std::string& input_transcription,
                        const std::string& output_transcription) override {}
  void OnAudioOutput(mojo_base::BigBuffer audio_data,
                     int64_t sequence_number) override {}
  void OnGenerationStateChanged(bool started,
                                bool completed,
                                bool interrupted) override {}

 private:
  mojo::Receiver<ai_overlay_dialog::mojom::Page> receiver_{this};
};

class AiOverlayToolsBrowserTest : public InProcessBrowserTest {
 public:
  AiOverlayToolsBrowserTest() {
    feature_list_.InitAndEnableFeatureWithParameters(features::kAiOverlayDialog,
                                                     {{"enable_next", "true"}});
  }

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        translate::switches::kTranslateScriptURL,
        embedded_test_server()->GetURL("/mock_translate_script.js").spec());
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &AiOverlayToolsBrowserTest::HandleRequest, base::Unretained(this)));
    embedded_test_server()->StartAcceptingConnections();

    mojo::PendingReceiver<ai_overlay_dialog::mojom::PageHandler>
        handler_receiver;
    page_handler_ = std::make_unique<AiOverlayDialogPageHandler>(
        std::move(handler_receiver), fake_page_.BindAndGetRemote(), browser());
    page_context_monitor_ =
        PageContextMonitor::Create(*browser(), *page_handler_);

    mojo::PendingRemote<ai_overlay_dialog::mojom::AiOverlayTools> remote;
    tools_ = AiOverlayTools::Create(remote.InitWithNewPipeAndPassReceiver(),
                                    browser(), page_context_monitor_.get());
  }

  void TearDownOnMainThread() override {
    tools_.reset();
    page_context_monitor_.reset();
    page_handler_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  AiOverlayTools* tools() { return tools_.get(); }
  PageContextMonitor* page_context_monitor() {
    return page_context_monitor_.get();
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    if (request.GetURL().GetPath() != "/mock_translate_script.js") {
      return nullptr;
    }

    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HTTP_OK);

    std::string script = R"JS(
      var google = {};
      google.translate = (function() {
        return {
          TranslateService: function() {
            return {
              isAvailable : function() { return true; },
              restore : function() { return; },
              getDetectedLanguage : function() { return "es"; },
              translatePage : function(sourceLang, targetLang,
                                       onTranslateProgress) {
                onTranslateProgress(100, true, false);
              }
            };
          }
        };
      })();
      cr.googleTranslate.onTranslateElementLoad();
    )JS";

    http_response->set_content(script);
    http_response->set_content_type("text/javascript");
    return std::move(http_response);
  }

  void AddTabWithTitle(const GURL& url, const std::string& title) {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url, WindowOpenDisposition::kNewForegroundTab,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    content::WebContents* contents =
        browser()->GetTabStripModel()->GetActiveWebContents();
    EXPECT_TRUE(content::ExecJs(contents, "document.title = '" + title + "'"));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  FakePage fake_page_;
  std::unique_ptr<AiOverlayDialogPageHandler> page_handler_;
  std::unique_ptr<PageContextMonitor> page_context_monitor_;
  std::unique_ptr<AiOverlayTools> tools_;
};

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, SwitchTabByTitle) {
  AddTabWithTitle(embedded_test_server()->GetURL("/empty.html?1"), "First Tab");
  AddTabWithTitle(embedded_test_server()->GetURL("/empty.html?2"),
                  "Second Tab");
  AddTabWithTitle(embedded_test_server()->GetURL("/empty.html?3"), "Third Tab");

  int initial_active = browser()->GetTabStripModel()->active_index();

  base::test::TestFuture<SwitchTabResult> future;
  tools()->SwitchTab("second", future.GetCallback());

  EXPECT_TRUE(future.Get().has_value());
  EXPECT_EQ(initial_active - 1, browser()->GetTabStripModel()->active_index());
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, SwitchTabByUrl) {
  AddTabWithTitle(embedded_test_server()->GetURL("/empty.html?1"), "First Tab");
  GURL target_url = embedded_test_server()->GetURL("/empty.html?target_path");
  AddTabWithTitle(target_url, "Second Tab");
  AddTabWithTitle(embedded_test_server()->GetURL("/empty.html?3"), "Third Tab");

  int initial_active = browser()->GetTabStripModel()->active_index();

  base::test::TestFuture<SwitchTabResult> future;
  tools()->SwitchTab("target_path", future.GetCallback());

  EXPECT_TRUE(future.Get().has_value());
  EXPECT_EQ(target_url, future.Get().value()->url);
  EXPECT_EQ(initial_active - 1, browser()->GetTabStripModel()->active_index());
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest,
                       SwitchTabBlocksNonHttpSchemes) {
  AddTabWithTitle(embedded_test_server()->GetURL("/empty.html?1"), "First Tab");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("chrome://version"),
      WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  AddTabWithTitle(embedded_test_server()->GetURL("/empty.html?3"), "Third Tab");

  base::test::TestFuture<SwitchTabResult> future;
  tools()->SwitchTab("version", future.GetCallback());

  EXPECT_FALSE(future.Get().has_value());
  EXPECT_EQ("No matching tab found", future.Get().error());
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, SwitchTabNotFound) {
  AddTabWithTitle(embedded_test_server()->GetURL("/empty.html?1"), "First Tab");

  base::test::TestFuture<SwitchTabResult> future;
  tools()->SwitchTab("nonexistent", future.GetCallback());

  EXPECT_FALSE(future.Get().has_value());
  EXPECT_EQ("No matching tab found", future.Get().error());
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, CloseCurrentTab) {
  AddTabWithTitle(embedded_test_server()->GetURL("/empty.html?1"), "First Tab");
  AddTabWithTitle(embedded_test_server()->GetURL("/empty.html?2"),
                  "Second Tab");

  int initial_count = browser()->GetTabStripModel()->count();

  base::test::TestFuture<CloseTabResult> future;
  tools()->CloseCurrentTab(future.GetCallback());

  EXPECT_TRUE(future.Get().has_value());
  EXPECT_EQ(initial_count - 1, browser()->GetTabStripModel()->count());
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, GoBack) {
  GURL first_url = embedded_test_server()->GetURL("/empty.html?1");
  GURL second_url = embedded_test_server()->GetURL("/empty.html?2");

  AddTabWithTitle(first_url, "First");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), second_url));

  content::WebContents* contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  content::NavigationController& controller = contents->GetController();

  EXPECT_TRUE(controller.CanGoBack());

  base::test::TestFuture<GoBackResult> future;
  tools()->GoBack(future.GetCallback());

  EXPECT_TRUE(future.Get().has_value());
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, GoBackCannotGoBack) {
  AddTabWithTitle(embedded_test_server()->GetURL("/empty.html?1"), "First");
  content::WebContents* contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  content::NavigationController& controller = contents->GetController();

  // Fresh tab has an initial navigation, wait wait, navigating once might add
  // an entry, but ui_test_utils::NavigateToURLWithDisposition replaces if
  // empty.
  EXPECT_FALSE(controller.CanGoBack());

  base::test::TestFuture<GoBackResult> future;
  tools()->GoBack(future.GetCallback());

  EXPECT_FALSE(future.Get().has_value());
  EXPECT_EQ("Cannot go back", future.Get().error());
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, GoForward) {
  GURL first_url = embedded_test_server()->GetURL("/empty.html?1");
  GURL second_url = embedded_test_server()->GetURL("/empty.html?2");

  AddTabWithTitle(first_url, "First");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), second_url));

  content::WebContents* contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  content::NavigationController& controller = contents->GetController();

  controller.GoBack();
  EXPECT_TRUE(content::WaitForLoadStop(contents));

  EXPECT_TRUE(controller.CanGoForward());

  base::test::TestFuture<GoForwardResult> future;
  tools()->GoForward(future.GetCallback());

  EXPECT_TRUE(future.Get().has_value());
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, GoForwardCannotGoForward) {
  AddTabWithTitle(embedded_test_server()->GetURL("/empty.html?1"), "First");
  content::WebContents* contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  content::NavigationController& controller = contents->GetController();

  EXPECT_FALSE(controller.CanGoForward());

  base::test::TestFuture<GoForwardResult> future;
  tools()->GoForward(future.GetCallback());

  EXPECT_FALSE(future.Get().has_value());
  EXPECT_EQ("Cannot go forward", future.Get().error());
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, ReloadPage) {
  AddTabWithTitle(embedded_test_server()->GetURL("/empty.html?1"), "First");

  base::test::TestFuture<ReloadResult> future;
  tools()->ReloadPage(future.GetCallback());

  EXPECT_TRUE(future.Get().has_value());
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, PerformSearch) {
  base::test::TestFuture<PerformSearchResult> future;
  tools()->PerformSearch("test query", /*new_tab=*/false, future.GetCallback());

  EXPECT_TRUE(future.Get().has_value());

  // Wait for navigation to complete. We just check the navigation works
  // as TemplateURLService provides the search URL.
}

// For media tools, we utilize the embedded test server's built-in
// video-with-metadata.html page to test active media sessions.

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, FindAndHighlightSuccess) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));

  base::test::TestFuture<FindAndHighlightResult> future;
  tools()->FindAndHighlight("This page has no title", future.GetCallback());

  EXPECT_TRUE(future.Get().has_value());
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, FindAndHighlightNotFound) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));

  base::test::TestFuture<FindAndHighlightResult> future;
  tools()->FindAndHighlight("missing text", future.GetCallback());

  EXPECT_FALSE(future.Get().has_value());
  EXPECT_EQ("No match found", future.Get().error());
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest,
                       FindAndHighlightBlocksNonHttpSchemes) {
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("chrome://version")));

  base::test::TestFuture<FindAndHighlightResult> future;
  tools()->FindAndHighlight("Google Chrome", future.GetCallback());

  EXPECT_FALSE(future.Get().has_value());
  EXPECT_EQ("FindAndHighlight is only supported on HTTP/HTTPS pages",
            future.Get().error());
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, PlayVideoNoMediaSession) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/empty.html")));

  base::test::TestFuture<PlayVideoResult> future;
  tools()->PlayVideo(future.GetCallback());

  EXPECT_FALSE(future.Get().has_value());
  EXPECT_EQ("No active media session", future.Get().error());
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, VideoControls) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/media/session/video-with-metadata.html")));

  content::WebContents* contents =
      browser()->GetTabStripModel()->GetActiveWebContents();

  // Start playback.
  ASSERT_EQ(base::Value(), content::EvalJs(contents, "play()"));

  // Pause video
  {
    base::test::TestFuture<PauseVideoResult> future;
    tools()->PauseVideo(future.GetCallback());
    EXPECT_TRUE(future.Get().has_value());
  }

  // Play video
  {
    base::test::TestFuture<PlayVideoResult> future;
    tools()->PlayVideo(future.GetCallback());
    EXPECT_TRUE(future.Get().has_value());
  }

  // Seek
  {
    base::test::TestFuture<SeekToTimestampResult> future;
    tools()->SeekToTimestamp("0:05", future.GetCallback());
    EXPECT_TRUE(future.Get().has_value());
  }
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, PauseVideoNoMediaSession) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/empty.html")));

  base::test::TestFuture<PauseVideoResult> future;
  tools()->PauseVideo(future.GetCallback());

  EXPECT_FALSE(future.Get().has_value());
  EXPECT_EQ("No active media session", future.Get().error());
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest,
                       SeekToTimestampNoMediaSession) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/empty.html")));

  base::test::TestFuture<SeekToTimestampResult> future;
  tools()->SeekToTimestamp("0:30", future.GetCallback());

  EXPECT_FALSE(future.Get().has_value());
  // The media session isn't available, so it fails with this error before
  // parsing timecode.
  EXPECT_EQ("No active media session", future.Get().error());
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, TranslatePageDefault) {
  content::WebContents* contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  auto lang_waiter = translate::CreateTranslateWaiter(
      contents, translate::TranslateWaiter::WaitEvent::kLanguageDetermined);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/empty.html")));
  lang_waiter->Wait();

  base::test::TestFuture<TranslatePageResult> future;
  tools()->TranslatePage("", future.GetCallback());

  EXPECT_TRUE(future.Get().has_value());

  // Wait for the translation to be processed by the mock script.
  translate::CreateTranslateWaiter(
      contents, translate::TranslateWaiter::WaitEvent::kPageTranslated)
      ->Wait();

  // Validate that the translation was invoked with the correct target language
  // by checking the active LanguageState.
  ChromeTranslateClient* translate_client =
      ChromeTranslateClient::FromWebContents(contents);
  ASSERT_TRUE(translate_client);

  std::string source_language;
  std::string expected_target_language;
  translate_client->GetTranslateLanguages(contents, &source_language,
                                          &expected_target_language,
                                          /*for_display=*/false);

  EXPECT_EQ(expected_target_language,
            translate_client->GetLanguageState().current_language());
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, TranslatePageSpecificTarget) {
  content::WebContents* contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  auto lang_waiter = translate::CreateTranslateWaiter(
      contents, translate::TranslateWaiter::WaitEvent::kLanguageDetermined);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/empty.html")));
  lang_waiter->Wait();

  base::test::TestFuture<TranslatePageResult> future;
  tools()->TranslatePage("fr", future.GetCallback());

  EXPECT_TRUE(future.Get().has_value());

  // Wait for the translation to be processed by the mock script.
  translate::CreateTranslateWaiter(
      contents, translate::TranslateWaiter::WaitEvent::kPageTranslated)
      ->Wait();

  // Validate that the translation was invoked with the correct target language
  // by checking the active LanguageState.
  ChromeTranslateClient* translate_client =
      ChromeTranslateClient::FromWebContents(contents);
  ASSERT_TRUE(translate_client);
  EXPECT_EQ("fr", translate_client->GetLanguageState().current_language());
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, FollowLink) {
  GURL target_url = embedded_test_server()->GetURL("/empty.html?target");
  std::string html = "<a href=\"" + target_url.spec() + "\">Click Link</a>";
  GURL page_url("data:text/html," + html);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));

  // Wait for the context monitor to fetch page content.
  while (!page_context_monitor()->last_page_content().has_value()) {
    base::RunLoop run_loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(100));
    run_loop.Run();
  }

  int hash = base::PersistentHash(target_url.spec()) % 10000;
  std::string hash_str = base::NumberToString(hash);

  ui_test_utils::UrlLoadObserver load_observer(target_url);

  base::test::TestFuture<FollowLinkResult> future;
  tools()->FollowLink(hash_str, future.GetCallback());
  EXPECT_TRUE(future.Get().has_value());

  load_observer.Wait();
  EXPECT_EQ(target_url, browser()
                            ->GetTabStripModel()
                            ->GetActiveWebContents()
                            ->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, FollowLinkWithHashSymbol) {
  GURL target_url = embedded_test_server()->GetURL("/empty.html?target");
  std::string html = "<a href=\"" + target_url.spec() + "\">Click Link</a>";
  GURL page_url("data:text/html," + html);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));

  // Wait for the context monitor to fetch page content.
  while (!page_context_monitor()->last_page_content().has_value()) {
    base::RunLoop run_loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(100));
    run_loop.Run();
  }

  int hash = base::PersistentHash(target_url.spec()) % 10000;
  std::string hash_str = "#" + base::NumberToString(hash);

  ui_test_utils::UrlLoadObserver load_observer(target_url);

  base::test::TestFuture<FollowLinkResult> future;
  tools()->FollowLink(hash_str, future.GetCallback());
  EXPECT_TRUE(future.Get().has_value());

  load_observer.Wait();
  EXPECT_EQ(target_url, browser()
                            ->GetTabStripModel()
                            ->GetActiveWebContents()
                            ->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, AddAndRemoveBookmark) {
  GURL active_url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), active_url));

  // Add bookmark for active tab via Mojo tool call.
  base::test::TestFuture<base::expected<std::monostate, std::string>> add_future;
  tools()->AddBookmark(add_future.GetCallback());
  EXPECT_TRUE(add_future.Get().has_value());

  // Verify in BookmarkModel.
  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(GetProfile());
  ASSERT_TRUE(model);
  auto nodes = model->GetNodesByURL(active_url);
  EXPECT_EQ(1u, nodes.size());

  // Remove bookmark for active tab via Mojo tool call.
  base::test::TestFuture<base::expected<std::monostate, std::string>> remove_future;
  tools()->RemoveBookmark(remove_future.GetCallback());
  EXPECT_TRUE(remove_future.Get().has_value());

  // Verify removed.
  nodes = model->GetNodesByURL(active_url);
  EXPECT_TRUE(nodes.empty());
}

namespace {

using ToolResultFuture =
    base::test::TestFuture<base::expected<std::string, std::string>>;

std::string GetToolResultAction(ToolResultFuture& future) {
  if (!future.Get().has_value()) {
    return "";
  }
  auto dict = base::JSONReader::ReadDict(future.Get().value(),
                                          base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (!dict) {
    return "";
  }
  const std::string* action = dict->FindString("action");
  return action ? *action : "";
}

std::string GetToolResultPathJson(ToolResultFuture& future,
                                  const std::string& path) {
  if (!future.Get().has_value()) {
    return "";
  }
  auto dict = base::JSONReader::ReadDict(future.Get().value(),
                                          base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (!dict) {
    return "";
  }
  const base::Value* val = dict->FindByDottedPath(path);
  if (!val) {
    return "";
  }
  return base::WriteJson(*val).value_or("");
}

}  // namespace

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, OpenPageSingleMatchAutoNavigate) {
  // 1. Setup two tabs: tab 0 = title1.html, tab 1 = title2.html
  GURL tab1_url = embedded_test_server()->GetURL("/title1.html");
  GURL tab2_url = embedded_test_server()->GetURL("/title2.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), tab1_url));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), tab2_url, WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  TabStripModel* tab_strip = browser()->GetTabStripModel();
  ASSERT_EQ(2, tab_strip->count());
  ASSERT_EQ(1, tab_strip->active_index());

  // 2. Execute Single Match OpenPage -> Expect auto-switch to tab 0
  ToolResultFuture future;
  tools()->OpenPage("Title1", future.GetCallback());

  EXPECT_EQ("switched_tab", GetToolResultAction(future));
  EXPECT_EQ(0, tab_strip->active_index());
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, OpenPageNegativeNoMatch) {
  GURL tab_url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), tab_url));

  TabStripModel* tab_strip = browser()->GetTabStripModel();
  int initial_active_index = tab_strip->active_index();

  ToolResultFuture future;
  tools()->OpenPage("NonExistentTopicKeyword", future.GetCallback());

  ASSERT_TRUE(future.Get().has_value());
  EXPECT_EQ("", GetToolResultAction(future));
  EXPECT_EQ(initial_active_index, tab_strip->active_index());

  EXPECT_EQ("[]", GetToolResultPathJson(future, "open_tabs"));
  EXPECT_EQ("[]", GetToolResultPathJson(future, "bookmarks"));
  EXPECT_EQ("[]", GetToolResultPathJson(future, "history"));
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, OpenPageMultiMatchDisambiguation) {
  // 1. Setup Open Tab & Bookmark entry for multi-match query
  GURL page_url = embedded_test_server()->GetURL("/title2.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));

  bookmarks::BookmarkModel* bm_model =
      BookmarkModelFactory::GetForBrowserContext(GetProfile());
  ASSERT_TRUE(bm_model);
  const bookmarks::BookmarkNode* other_node = bm_model->other_node();
  bm_model->AddNewURL(other_node, 0, u"Title2 Bookmark", page_url);

  // 2. Execute Multi Match OpenPage -> Expect candidate listing with monotonically
  // assigned target_ids across categories (open_tabs candidates first, then
  // bookmarks, then history).
  ToolResultFuture future;
  tools()->OpenPage("Title2", future.GetCallback());

  ASSERT_TRUE(future.Get().has_value());
  EXPECT_EQ("", GetToolResultAction(future));

  auto dict = base::JSONReader::ReadDict(
      future.Get().value(), base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(dict.has_value());

  // Category 1: open_tabs with candidate target_id = 1
  const base::ListValue* open_tabs = dict->FindList("open_tabs");
  ASSERT_TRUE(open_tabs);
  ASSERT_FALSE(open_tabs->empty());
  EXPECT_EQ(1, (*open_tabs)[0].GetDict().FindInt("target_id"));

  // Category 2: bookmarks with candidate target_id = 2
  const base::ListValue* bookmarks = dict->FindList("bookmarks");
  ASSERT_TRUE(bookmarks);
  ASSERT_FALSE(bookmarks->empty());
  EXPECT_EQ(2, (*bookmarks)[0].GetDict().FindInt("target_id"));
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, OpenPageHistorySearch) {
  GURL history_url = embedded_test_server()->GetURL("/title3.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), history_url));

  GURL active_url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), active_url));

  ToolResultFuture future;
  tools()->OpenPage("Title3", future.GetCallback());

  EXPECT_EQ("opened_history", GetToolResultAction(future));
  EXPECT_EQ(
      history_url,
      browser()->GetTabStripModel()->GetActiveWebContents()->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, OpenPageSearchByTitle) {
  // 1. Setup tab 0 with a unique page title that does not appear in its URL.
  GURL tab0_url = embedded_test_server()->GetURL("/empty.html?0");
  GURL tab1_url = embedded_test_server()->GetURL("/empty.html?1");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), tab0_url));
  EXPECT_TRUE(
      content::ExecJs(browser()->GetTabStripModel()->GetActiveWebContents(),
                      "document.title = 'Unique Special Page Title'"));
  AddTabWithTitle(tab1_url, "Other Page");

  TabStripModel* tab_strip = browser()->GetTabStripModel();
  ASSERT_EQ(2, tab_strip->count());
  ASSERT_EQ(1, tab_strip->active_index());

  // 2. Search by page title query -> Expect auto-switch to tab 0
  ToolResultFuture future;
  tools()->OpenPage("Unique Special", future.GetCallback());

  EXPECT_EQ("switched_tab", GetToolResultAction(future));
  EXPECT_EQ(0, tab_strip->active_index());
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, GetToolDefinitions) {
  mojo::Remote<ai_overlay_dialog::mojom::AiOverlayToolRegistry> registry_remote;
  tools()->BindRegistryReceiver(registry_remote.BindNewPipeAndPassReceiver());

  base::test::TestFuture<const std::string&> future;
  registry_remote->GetToolDefinitions(future.GetCallback());

  std::string json_res = future.Get();
  auto list = base::JSONReader::ReadList(json_res, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(list.has_value());
  ASSERT_FALSE(list->empty());

  // Verify formatted JSON string contains full functionDeclarations schema
  auto tool_list_str = base::WriteJsonWithOptions(*list, base::OPTIONS_PRETTY_PRINT);
  ASSERT_TRUE(tool_list_str.has_value());
  EXPECT_FALSE(tool_list_str->empty());
  EXPECT_NE(std::string::npos, tool_list_str->find("functionDeclarations"));
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, SetTextSuccess) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      GURL("data:text/html,<html><body><input id='test_input' "
           "value='old' /></body></html>")));

  content::WebContents* contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  int dom_node_id =
      content::GetDOMNodeId(*contents->GetPrimaryMainFrame(), "#test_input")
          .value();

  base::test::TestFuture<base::expected<std::monostate, std::string>> future;
  tools()->SetText(blink::DOMNodeIdType(dom_node_id), "new_value",
                   future.GetCallback());

  EXPECT_TRUE(future.Get().has_value());
  EXPECT_EQ("new_value",
            content::EvalJs(contents, "document.getElementById('test_input').value"));
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, SetTextNotFound) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      GURL("data:text/html,<html><body><input id='test_input' "
           "value='old' /></body></html>")));

  base::test::TestFuture<base::expected<std::monostate, std::string>> future;
  tools()->SetText(blink::DOMNodeIdType(999), "new_value",
                   future.GetCallback());

  EXPECT_FALSE(future.Get().has_value());
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, ClickElementCheckbox) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      GURL("data:text/html,<html><body><input type='checkbox' id='test_check' "
           "/></body></html>")));

  content::WebContents* contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  int dom_node_id =
      content::GetDOMNodeId(*contents->GetPrimaryMainFrame(), "#test_check")
          .value();

  base::test::TestFuture<base::expected<std::monostate, std::string>> future;
  tools()->ClickElement(blink::DOMNodeIdType(dom_node_id),
                        future.GetCallback());

  EXPECT_TRUE(future.Get().has_value());
  EXPECT_EQ(true,
            content::EvalJs(contents, "document.getElementById('test_check').checked"));
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, ClickElementRadioButton) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      GURL("data:text/html,<html><body><input type='radio' id='test_radio' "
           "/></body></html>")));

  content::WebContents* contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  int dom_node_id =
      content::GetDOMNodeId(*contents->GetPrimaryMainFrame(), "#test_radio")
          .value();

  base::test::TestFuture<base::expected<std::monostate, std::string>> future;
  tools()->ClickElement(blink::DOMNodeIdType(dom_node_id),
                        future.GetCallback());

  EXPECT_TRUE(future.Get().has_value());
  EXPECT_EQ(true,
            content::EvalJs(contents, "document.getElementById('test_radio').checked"));
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, SetFullscreen) {
  base::test::TestFuture<base::expected<std::monostate, std::string>> future;
  tools()->SetFullscreen(true, future.GetCallback());
  EXPECT_TRUE(future.Get().has_value());
  EXPECT_TRUE(browser()->GetWindow()->IsFullscreen());

  base::test::TestFuture<base::expected<std::monostate, std::string>> future2;
  tools()->SetFullscreen(false, future2.GetCallback());
  EXPECT_TRUE(future2.Get().has_value());
  EXPECT_FALSE(browser()->GetWindow()->IsFullscreen());
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, SelectOptionSuccess) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      GURL("data:text/html,<html><body><select id='test_select'><option "
           "value='opt1'>Opt 1</option><option value='opt2'>Opt 2</option></select></body></html>")));

  content::WebContents* contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  int dom_node_id =
      content::GetDOMNodeId(*contents->GetPrimaryMainFrame(), "#test_select")
          .value();

  base::test::TestFuture<base::expected<std::monostate, std::string>> future;
  tools()->SelectOption(blink::DOMNodeIdType(dom_node_id), "opt2",
                        future.GetCallback());

  EXPECT_TRUE(future.Get().has_value());
  EXPECT_EQ("opt2",
            content::EvalJs(contents, "document.getElementById('test_select').value"));
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, ManageWorkspaceMultipleTabs) {
  // Setup multiple diverse tabs in the window.
  AddTabWithTitle(embedded_test_server()->GetURL("/empty.html?cookies"),
                  "Chocolate Chip Cookies");
  AddTabWithTitle(embedded_test_server()->GetURL("/empty.html?quantum"),
                  "Quantum Mechanics");
  AddTabWithTitle(embedded_test_server()->GetURL("/empty.html?sourdough"),
                  "Sourdough Bread");

  int initial_tab_count = browser()->GetTabStripModel()->count();
  EXPECT_GE(initial_tab_count, 3);

  // Verify that BuildWorkspaceSnapshot serializes all windows and open tabs.
  base::DictValue snapshot = tools()->BuildWorkspaceSnapshot();

  const base::ListValue* windows = snapshot.FindList("windows");
  ASSERT_TRUE(windows);
  ASSERT_FALSE(windows->empty());
  EXPECT_TRUE(snapshot.FindList("saved_tab_groups"));

  const base::DictValue& first_window = (*windows)[0].GetDict();
  EXPECT_EQ(first_window.FindInt("tab_count"), initial_tab_count);
  const base::ListValue* tabs = first_window.FindList("tabs");
  ASSERT_TRUE(tabs);
  EXPECT_EQ(static_cast<int>(tabs->size()), initial_tab_count);

  bool found_cookies = false;
  bool found_quantum = false;
  bool found_sourdough = false;
  for (const base::Value& tab_val : *tabs) {
    ASSERT_TRUE(tab_val.is_dict());
    const base::DictValue& tab_dict = tab_val.GetDict();
    EXPECT_TRUE(tab_dict.FindInt("tab_id").has_value());
    EXPECT_TRUE(tab_dict.FindInt("memory_mb").has_value());
    EXPECT_TRUE(tab_dict.FindInt("last_active_minutes_ago").has_value());
    EXPECT_TRUE(tab_dict.FindBool("has_unsaved_state").has_value());
    const std::string* title = tab_dict.FindString("title");
    ASSERT_TRUE(title);
    if (*title == "Chocolate Chip Cookies") {
      found_cookies = true;
    } else if (*title == "Quantum Mechanics") {
      found_quantum = true;
    } else if (*title == "Sourdough Bread") {
      found_sourdough = true;
    }
  }
  EXPECT_TRUE(found_cookies);
  EXPECT_TRUE(found_quantum);
  EXPECT_TRUE(found_sourdough);

  // Invoke AnalyzeAndManageTabs and verify callback completion with snapshot.
  base::test::TestFuture<base::expected<base::Value, std::string>> future;
  tools()->AnalyzeAndManageTabs("What tabs do I have open about cooking?",
                                future.GetCallback());

  const auto& result = future.Get();
  ASSERT_TRUE(result.has_value()) << (result.has_value() ? "" : result.error());
  ASSERT_TRUE(result.value().is_dict());
  const base::ListValue* result_windows =
      result.value().GetDict().FindList("windows");
  ASSERT_TRUE(result_windows);
  EXPECT_FALSE(result_windows->empty());
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest,
                       ManageWorkspaceDefaultSystemInstructionAndActions) {
  AddTabWithTitle(embedded_test_server()->GetURL("/empty.html?cookies"),
                  "Chocolate Chip Cookies");
  AddTabWithTitle(embedded_test_server()->GetURL("/empty.html?sourdough"),
                  "Sourdough Bread");

  TabStripModel* tab_strip = browser()->GetTabStripModel();
  ASSERT_TRUE(tab_strip->group_model());
  EXPECT_TRUE(tab_strip->group_model()->ListTabGroups().empty());

  int cookies_tab_id = -1;
  int sourdough_tab_id = -1;
  for (int i = 0; i < tab_strip->count(); ++i) {
    content::WebContents* wc = tab_strip->GetWebContentsAt(i);
    std::string title = base::UTF16ToUTF8(wc->GetTitle());
    int tid = sessions::SessionTabHelper::IdForTab(wc).id();
    if (title == "Chocolate Chip Cookies") {
      cookies_tab_id = tid;
    } else if (title == "Sourdough Bread") {
      sourdough_tab_id = tid;
    }
  }
  ASSERT_GT(cookies_tab_id, 0);
  ASSERT_GT(sourdough_tab_id, 0);

  const std::string fenced_subagent_output = base::StringPrintf(
      "```json\n{\n  \"facts\": \"Grouped baking tabs.\",\n  \"actions\": [\n"
      "    {\"type\": \"create_group\", \"title\": \"Baking\", \"color\": "
      "\"orange\", \"tab_ids\": [%d, %d]}\n  ]\n}\n```",
      cookies_tab_id, sourdough_tab_id);

  base::test::TestFuture<base::expected<base::Value, std::string>> future;
  tools()->AnalyzeAndManageTabs(fenced_subagent_output, future.GetCallback());

  const auto& result = future.Get();
  ASSERT_TRUE(result.has_value()) << (result.has_value() ? "" : result.error());

  // 1. Verify that the create_group action was executed in TabStripModel.
  EXPECT_FALSE(tab_strip->group_model()->ListTabGroups().empty());
  tab_groups::TabGroupId created_gid =
      tab_strip->group_model()->ListTabGroups()[0];
  TabGroup* grp = tab_strip->group_model()->GetTabGroup(created_gid);
  ASSERT_TRUE(grp);
  EXPECT_EQ(grp->visual_data()->title(), u"Baking");

  // 2. Verify returned dictionary contains actions_taken.
  ASSERT_TRUE(result->is_dict());
  const base::ListValue* actions_taken =
      result->GetDict().FindList("actions_taken");
  ASSERT_TRUE(actions_taken);
  EXPECT_EQ(actions_taken->size(), 1u);
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, ManageWorkspaceGroupAndUndo) {
  AddTabWithTitle(embedded_test_server()->GetURL("/empty.html?cookies"),
                  "Chocolate Chip Cookies");
  AddTabWithTitle(embedded_test_server()->GetURL("/empty.html?quantum"),
                  "Quantum Mechanics");
  AddTabWithTitle(embedded_test_server()->GetURL("/empty.html?sourdough"),
                  "Sourdough Bread");

  TabStripModel* tab_strip = browser()->GetTabStripModel();
  ASSERT_TRUE(tab_strip->group_model());
  EXPECT_TRUE(tab_strip->group_model()->ListTabGroups().empty());

  int cookies_tab_id = -1;
  int quantum_tab_id = -1;
  int sourdough_tab_id = -1;
  for (int i = 0; i < tab_strip->count(); ++i) {
    content::WebContents* wc = tab_strip->GetWebContentsAt(i);
    std::string title = base::UTF16ToUTF8(wc->GetTitle());
    int tid = sessions::SessionTabHelper::IdForTab(wc).id();
    if (title == "Chocolate Chip Cookies") {
      cookies_tab_id = tid;
    } else if (title == "Quantum Mechanics") {
      quantum_tab_id = tid;
    } else if (title == "Sourdough Bread") {
      sourdough_tab_id = tid;
    }
  }
  ASSERT_GT(cookies_tab_id, 0);
  ASSERT_GT(quantum_tab_id, 0);
  ASSERT_GT(sourdough_tab_id, 0);

  // 1. Execute create_group action for the two baking tabs.
  std::string create_group_payload = base::StringPrintf(
      R"({
        "facts": "Grouped 2 baking tabs into 'Baking'.",
        "actions": [
          {
            "type": "create_group",
            "title": "Baking",
            "color": "orange",
            "tab_ids": [%d, %d]
          }
        ]
      })",
      cookies_tab_id, sourdough_tab_id);

  base::DictValue result_dict =
      tools()->ExecuteWorkspaceSubAgentOutput(create_group_payload);
  EXPECT_EQ(*result_dict.FindString("facts"),
            "Grouped 2 baking tabs into 'Baking'.");
  const base::ListValue* actions_taken = result_dict.FindList("actions_taken");
  ASSERT_TRUE(actions_taken);
  ASSERT_EQ(actions_taken->size(), 1u);
  EXPECT_EQ(*(*actions_taken)[0].GetDict().FindString("type"), "create_group");
  EXPECT_EQ((*actions_taken)[0].GetDict().FindInt("tab_count"), 2);

  std::vector<tab_groups::TabGroupId> groups =
      tab_strip->group_model()->ListTabGroups();
  ASSERT_EQ(groups.size(), 1u);
  TabGroup* baking_group = tab_strip->group_model()->GetTabGroup(groups[0]);
  ASSERT_TRUE(baking_group);
  EXPECT_EQ(base::UTF16ToUTF8(baking_group->visual_data()->title()), "Baking");
  EXPECT_EQ(baking_group->tab_count(), 2);

  // 2. Undo the last action and verify all tabs are ungrouped again.
  std::string undo_payload = R"({
    "facts": "Undid tab grouping.",
    "actions": [{"type": "undo_last_action"}]
  })";
  base::DictValue undo_dict =
      tools()->ExecuteWorkspaceSubAgentOutput(undo_payload);
  const base::ListValue* undo_taken = undo_dict.FindList("actions_taken");
  ASSERT_TRUE(undo_taken);
  ASSERT_EQ(undo_taken->size(), 1u);
  EXPECT_EQ(*(*undo_taken)[0].GetDict().FindString("type"), "undo_last_action");

  for (int i = 0; i < tab_strip->count(); ++i) {
    EXPECT_FALSE(tab_strip->GetTabAtIndex(i)->GetGroup().has_value());
  }
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest,
                       ManageWorkspaceCloseTabsSkipsUnsavedAndUndoes) {
  AddTabWithTitle(embedded_test_server()->GetURL("/empty.html?clean"),
                  "Clean Tab");
  AddTabWithTitle(embedded_test_server()->GetURL("/empty.html?unsaved"),
                  "Unsaved Draft Tab");

  TabStripModel* tab_strip = browser()->GetTabStripModel();
  int initial_count = tab_strip->count();

  int clean_tab_id = -1;
  int unsaved_tab_id = -1;
  content::WebContents* unsaved_wc = nullptr;
  for (int i = 0; i < tab_strip->count(); ++i) {
    content::WebContents* wc = tab_strip->GetWebContentsAt(i);
    std::string title = base::UTF16ToUTF8(wc->GetTitle());
    int tid = sessions::SessionTabHelper::IdForTab(wc).id();
    if (title == "Clean Tab") {
      clean_tab_id = tid;
    } else if (title == "Unsaved Draft Tab") {
      unsaved_tab_id = tid;
      unsaved_wc = wc;
      // Attach a beforeunload listener with user activation so
      // WebContentsHasUnsavedState() returns true.
      EXPECT_TRUE(content::ExecJs(
          wc,
          "window._buHandler = (e) => { e.preventDefault(); }; "
          "window.addEventListener('beforeunload', window._buHandler);"));
    }
  }
  ASSERT_GT(clean_tab_id, 0);
  ASSERT_GT(unsaved_tab_id, 0);
  ASSERT_TRUE(unsaved_wc);

  std::string close_payload = base::StringPrintf(
      R"({
        "facts": "Closed 1 tab; skipped 1 tab with unsaved changes.",
        "actions": [
          {
            "type": "close_tabs",
            "tab_ids": [%d, %d]
          }
        ]
      })",
      clean_tab_id, unsaved_tab_id);

  base::DictValue result_dict =
      tools()->ExecuteWorkspaceSubAgentOutput(close_payload);

  // Clean Tab should be closed; Unsaved Draft Tab must be skipped and remain
  // open.
  EXPECT_EQ(tab_strip->count(), initial_count - 1);
  const base::ListValue* skipped = result_dict.FindList("skipped");
  ASSERT_TRUE(skipped);
  ASSERT_EQ(skipped->size(), 1u);
  EXPECT_EQ((*skipped)[0].GetDict().FindInt("tab_id"), unsaved_tab_id);
  EXPECT_EQ(*(*skipped)[0].GetDict().FindString("reason"), "has_unsaved_state");

  // Undo the close action and verify "Clean Tab" is restored.
  std::string undo_payload = R"({
    "facts": "Restored closed tab.",
    "actions": [{"type": "undo_last_action"}]
  })";
  base::DictValue undo_dict =
      tools()->ExecuteWorkspaceSubAgentOutput(undo_payload);
  const base::ListValue* undo_taken = undo_dict.FindList("actions_taken");
  ASSERT_TRUE(undo_taken);
  ASSERT_EQ(undo_taken->size(), 1u);
  EXPECT_EQ(*(*undo_taken)[0].GetDict().FindString("type"), "undo_last_action");
  EXPECT_EQ(tab_strip->count(), initial_count);

  // Remove the beforeunload listener from unsaved_wc before teardown so
  // QuitBrowsers succeeds without prompting.
  EXPECT_TRUE(content::ExecJs(
      unsaved_wc,
      "if (window._buHandler) { "
      "  window.removeEventListener('beforeunload', window._buHandler); "
      "  delete window._buHandler; "
      "} "
      "window.onbeforeunload = null;"));
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest,
                       ManageWorkspaceWindowActions) {
  AddTabWithTitle(embedded_test_server()->GetURL("/empty.html?tab1"), "Tab 1");
  AddTabWithTitle(embedded_test_server()->GetURL("/empty.html?tab2"), "Tab 2");

  TabStripModel* tab_strip = browser()->GetTabStripModel();
  int tab1_id = -1;
  int tab2_id = -1;
  for (int i = 0; i < tab_strip->count(); ++i) {
    content::WebContents* wc = tab_strip->GetWebContentsAt(i);
    std::string title = base::UTF16ToUTF8(wc->GetTitle());
    int tid = sessions::SessionTabHelper::IdForTab(wc).id();
    if (title == "Tab 1") {
      tab1_id = tid;
    } else if (title == "Tab 2") {
      tab2_id = tid;
    }
  }
  ASSERT_GT(tab1_id, 0);
  ASSERT_GT(tab2_id, 0);

  // 1. Move Tab 1 to a new window.
  std::string new_win_payload = base::StringPrintf(
      R"({
        "facts": "Moved Tab 1 to a new window.",
        "actions": [
          {
            "type": "move_to_new_window",
            "group_title": "Project Alpha",
            "color": "blue",
            "tab_ids": [%d]
          }
        ]
      })",
      tab1_id);

  base::DictValue result_dict =
      tools()->ExecuteWorkspaceSubAgentOutput(new_win_payload);
  const base::ListValue* actions_taken = result_dict.FindList("actions_taken");
  ASSERT_TRUE(actions_taken);
  ASSERT_EQ(actions_taken->size(), 1u);
  EXPECT_EQ(*(*actions_taken)[0].GetDict().FindString("type"),
            "move_to_new_window");
  EXPECT_EQ((*actions_taken)[0].GetDict().FindInt("tab_count"), 1);

  // Verify there are now 2 windows open.
  int window_count = 0;
  ProfileBrowserCollection::GetForProfile(browser()->GetProfile())
      ->ForEach([&](BrowserWindowInterface* bwi) {
        if (bwi->GetTabStripModel()) {
          ++window_count;
        }
        return true;
      });
  ASSERT_EQ(window_count, 2);
  EXPECT_EQ(browser()->GetTabStripModel()->count(), 2);

  // 2. Merge the newly-shown window (window_index: 1 in activation order) back
  // into the primary browser window (window_index: 2).
  std::string merge_payload = R"({
    "facts": "Merged secondary window back into primary window.",
    "actions": [
      {
        "type": "merge_windows",
        "source_window": 1,
        "target_window": 2
      }
    ]
  })";

  base::DictValue merge_dict =
      tools()->ExecuteWorkspaceSubAgentOutput(merge_payload);
  const base::ListValue* merge_taken = merge_dict.FindList("actions_taken");
  ASSERT_TRUE(merge_taken);
  ASSERT_EQ(merge_taken->size(), 1u);
  EXPECT_EQ(*(*merge_taken)[0].GetDict().FindString("type"), "merge_windows");
  EXPECT_EQ(browser()->GetTabStripModel()->count(), 3);
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest,
                       ManageWorkspaceReorderTabsAndUndo) {
  // Setup 3 tabs: Tab 0 (A), Tab 1 (B), Tab 2 (C).
  AddTabWithTitle(embedded_test_server()->GetURL("/empty.html?a"), "Tab A");
  AddTabWithTitle(embedded_test_server()->GetURL("/empty.html?b"), "Tab B");

  TabStripModel* tab_strip = browser()->GetTabStripModel();
  ASSERT_EQ(tab_strip->count(), 3);

  int id_0 =
      sessions::SessionTabHelper::IdForTab(tab_strip->GetWebContentsAt(0)).id();
  int id_1 =
      sessions::SessionTabHelper::IdForTab(tab_strip->GetWebContentsAt(1)).id();
  int id_2 =
      sessions::SessionTabHelper::IdForTab(tab_strip->GetWebContentsAt(2)).id();

  // Reorder to [id_2, id_0, id_1] (C, A, B).
  std::string reorder_payload = base::StringPrintf(
      R"({
    "facts": "Reordered tabs.",
    "actions": [
      {
        "type": "reorder_tabs",
        "tab_ids": [%d, %d, %d]
      }
    ]
  })",
      id_2, id_0, id_1);

  base::DictValue result_dict =
      tools()->ExecuteWorkspaceSubAgentOutput(reorder_payload);
  const base::ListValue* actions_taken = result_dict.FindList("actions_taken");
  ASSERT_TRUE(actions_taken);
  ASSERT_EQ(actions_taken->size(), 1u);
  EXPECT_EQ(*(*actions_taken)[0].GetDict().FindString("type"), "reorder_tabs");

  // Verify new order in tab strip.
  EXPECT_EQ(
      sessions::SessionTabHelper::IdForTab(tab_strip->GetWebContentsAt(0)).id(),
      id_2);
  EXPECT_EQ(
      sessions::SessionTabHelper::IdForTab(tab_strip->GetWebContentsAt(1)).id(),
      id_0);
  EXPECT_EQ(
      sessions::SessionTabHelper::IdForTab(tab_strip->GetWebContentsAt(2)).id(),
      id_1);

  // Undo the reordering.
  std::string undo_payload = R"({
    "facts": "Undid last action.",
    "actions": [
      {
        "type": "undo_last_action"
      }
    ]
  })";

  base::DictValue undo_dict =
      tools()->ExecuteWorkspaceSubAgentOutput(undo_payload);
  const base::ListValue* undo_taken = undo_dict.FindList("actions_taken");
  ASSERT_TRUE(undo_taken);
  ASSERT_EQ(undo_taken->size(), 1u);
  EXPECT_EQ(*(*undo_taken)[0].GetDict().FindString("type"), "undo_last_action");

  // Verify order is restored: [id_0, id_1, id_2].
  EXPECT_EQ(
      sessions::SessionTabHelper::IdForTab(tab_strip->GetWebContentsAt(0)).id(),
      id_0);
  EXPECT_EQ(
      sessions::SessionTabHelper::IdForTab(tab_strip->GetWebContentsAt(1)).id(),
      id_1);
  EXPECT_EQ(
      sessions::SessionTabHelper::IdForTab(tab_strip->GetWebContentsAt(2)).id(),
      id_2);
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest,
                       ManageWorkspaceReorderTabsWithGroupsAndUndo) {
  // Setup 4 tabs: Tab 0 (A), Tab 1 (B), Tab 2 (C), Tab 3 (D).
  AddTabWithTitle(embedded_test_server()->GetURL("/empty.html?a"), "Tab A");
  AddTabWithTitle(embedded_test_server()->GetURL("/empty.html?b"), "Tab B");
  AddTabWithTitle(embedded_test_server()->GetURL("/empty.html?c"), "Tab C");

  TabStripModel* tab_strip = browser()->GetTabStripModel();
  ASSERT_EQ(tab_strip->count(), 4);

  // Group tabs 1 and 2 (B and C).
  tab_groups::TabGroupId group_id = tab_strip->AddToNewGroup({1, 2});
  ASSERT_TRUE(tab_strip->group_model()->ContainsTabGroup(group_id));

  int id_0 =
      sessions::SessionTabHelper::IdForTab(tab_strip->GetWebContentsAt(0)).id();
  int id_1 =
      sessions::SessionTabHelper::IdForTab(tab_strip->GetWebContentsAt(1)).id();
  int id_2 =
      sessions::SessionTabHelper::IdForTab(tab_strip->GetWebContentsAt(2)).id();
  int id_3 =
      sessions::SessionTabHelper::IdForTab(tab_strip->GetWebContentsAt(3)).id();

  // Reorder tabs across grouped and ungrouped tabs: [id_3, id_2, id_1, id_0].
  // This verifies that MoveWebContentsAt does not trigger CHECK crashes in
  // TabCollection.
  std::string reorder_payload = base::StringPrintf(
      R"({
    "facts": "Reordered tabs with groups.",
    "actions": [
      {
        "type": "reorder_tabs",
        "tab_ids": [%d, %d, %d, %d]
      }
    ]
  })",
      id_3, id_2, id_1, id_0);

  base::DictValue result_dict =
      tools()->ExecuteWorkspaceSubAgentOutput(reorder_payload);
  const base::ListValue* actions_taken = result_dict.FindList("actions_taken");
  ASSERT_TRUE(actions_taken);
  ASSERT_EQ(actions_taken->size(), 1u);
  EXPECT_EQ(*(*actions_taken)[0].GetDict().FindString("type"), "reorder_tabs");

  // Undo the reordering.
  std::string undo_payload = R"({
    "facts": "Undid last action.",
    "actions": [
      {
        "type": "undo_last_action"
      }
    ]
  })";

  base::DictValue undo_dict =
      tools()->ExecuteWorkspaceSubAgentOutput(undo_payload);
  const base::ListValue* undo_taken = undo_dict.FindList("actions_taken");
  ASSERT_TRUE(undo_taken);
  ASSERT_EQ(undo_taken->size(), 1u);
  EXPECT_EQ(*(*undo_taken)[0].GetDict().FindString("type"), "undo_last_action");

  EXPECT_EQ(
      sessions::SessionTabHelper::IdForTab(tab_strip->GetWebContentsAt(0)).id(),
      id_0);
  EXPECT_EQ(
      sessions::SessionTabHelper::IdForTab(tab_strip->GetWebContentsAt(1)).id(),
      id_1);
  EXPECT_EQ(
      sessions::SessionTabHelper::IdForTab(tab_strip->GetWebContentsAt(2)).id(),
      id_2);
  EXPECT_EQ(
      sessions::SessionTabHelper::IdForTab(tab_strip->GetWebContentsAt(3)).id(),
      id_3);
  EXPECT_FALSE(tab_strip->GetTabAtIndex(0)->GetGroup().has_value());
  EXPECT_EQ(tab_strip->GetTabAtIndex(1)->GetGroup(), group_id);
  EXPECT_EQ(tab_strip->GetTabAtIndex(2)->GetGroup(), group_id);
  EXPECT_FALSE(tab_strip->GetTabAtIndex(3)->GetGroup().has_value());
}

IN_PROC_BROWSER_TEST_F(
    AiOverlayToolsBrowserTest,
    ManageWorkspaceCloseGroupedTabAndRestorePreservesGroupAndOrder) {
  AddTabWithTitle(embedded_test_server()->GetURL("/empty.html?tab_a"), "Tab A");
  AddTabWithTitle(embedded_test_server()->GetURL("/empty.html?tab_b"), "Tab B");
  AddTabWithTitle(embedded_test_server()->GetURL("/empty.html?tab_c"), "Tab C");

  TabStripModel* tab_strip = browser()->GetTabStripModel();
  ASSERT_EQ(tab_strip->count(), 4);

  int tab_b_id =
      sessions::SessionTabHelper::IdForTab(tab_strip->GetWebContentsAt(2)).id();
  int tab_c_id =
      sessions::SessionTabHelper::IdForTab(tab_strip->GetWebContentsAt(3)).id();

  // 1. Group Tab B and Tab C into "Research".
  tools()->ExecuteWorkspaceSubAgentOutput(base::StringPrintf(
      R"({
        "facts": "Grouped Research tabs.",
        "actions": [
          {"type": "create_group", "title": "Research", "color": "purple", "tab_ids": [%d, %d]}
        ]
      })",
      tab_b_id, tab_c_id));
  ASSERT_EQ(tab_strip->group_model()->ListTabGroups().size(), 1u);
  tab_groups::TabGroupId research_gid =
      tab_strip->group_model()->ListTabGroups()[0];
  EXPECT_EQ(tab_strip->GetTabAtIndex(2)->GetGroup(), research_gid);
  EXPECT_EQ(tab_strip->GetTabAtIndex(3)->GetGroup(), research_gid);

  // 2. Close Tab B (index 2).
  tools()->ExecuteWorkspaceSubAgentOutput(base::StringPrintf(
      R"({
        "facts": "Closed Tab B.",
        "actions": [{"type": "close_tabs", "tab_ids": [%d]}]
      })",
      tab_b_id));
  ASSERT_EQ(tab_strip->count(), 3);

  // 3. Execute a read-only output and a completely skipped close_tabs action;
  // neither should clobber the undo snapshot.
  tools()->ExecuteWorkspaceSubAgentOutput(R"({"facts": "Read-only query."})");
  tools()->ExecuteWorkspaceSubAgentOutput(
      R"({"facts": "Skipped.", "actions": [{"type": "close_tabs", "tab_ids": [999999]}]})");

  // 4. Undo the close_tabs action and verify Tab B is restored at index 2 and
  // re-associated with the "Research" group even though TabRestoreService
  // assigned a new SessionTabHelper ID.
  base::DictValue undo_dict = tools()->ExecuteWorkspaceSubAgentOutput(
      R"({"facts": "Undid close.", "actions": [{"type": "undo_last_action"}]})");
  ASSERT_TRUE(undo_dict.FindList("actions_taken"));
  ASSERT_EQ(tab_strip->count(), 4);
  EXPECT_EQ(tab_strip->GetWebContentsAt(2)->GetTitle(), u"Tab B");
  EXPECT_EQ(tab_strip->GetTabAtIndex(2)->GetGroup(), research_gid);
  EXPECT_EQ(tab_strip->GetTabAtIndex(3)->GetGroup(), research_gid);
}

IN_PROC_BROWSER_TEST_F(
    AiOverlayToolsBrowserTest,
    ManageWorkspaceActivateCollapsedGroupPinMuteAndFastPath) {
  AddTabWithTitle(embedded_test_server()->GetURL("/empty.html?music"),
                  "Music Player");
  AddTabWithTitle(embedded_test_server()->GetURL("/empty.html?docs"),
                  "Design Doc");

  TabStripModel* tab_strip = browser()->GetTabStripModel();
  ASSERT_EQ(tab_strip->count(), 3);

  int music_tab_id =
      sessions::SessionTabHelper::IdForTab(tab_strip->GetWebContentsAt(1)).id();
  int docs_tab_id =
      sessions::SessionTabHelper::IdForTab(tab_strip->GetWebContentsAt(2)).id();

  // Put "Design Doc" in a collapsed group and activate tab 0.
  tab_groups::TabGroupId gid = tab_strip->AddToNewGroup({2});
  tab_strip->ChangeTabGroupVisuals(
      gid,
      tab_groups::TabGroupVisualData(
          u"Docs", tab_groups::TabGroupColorId::kCyan, /*is_collapsed=*/true),
      /*is_customized=*/true);
  tab_strip->ActivateTabAt(0);
  ASSERT_TRUE(tab_strip->group_model()
                  ->GetTabGroup(gid)
                  ->visual_data()
                  ->is_collapsed());

  // 1. Activate "Design Doc" via the direct structured JSON fast-path on
  // AnalyzeAndManageTabs; verify it activates the tab AND expands the collapsed
  // group without requiring an LLM network call.
  base::test::TestFuture<base::expected<base::Value, std::string>> act_future;
  tools()->AnalyzeAndManageTabs(
      base::StringPrintf(
          R"({"facts": "Activated Design Doc.", "actions": [{"type": "activate_tab", "tab_id": %d}]})",
          docs_tab_id),
      act_future.GetCallback());
  ASSERT_TRUE(act_future.Get().has_value());
  EXPECT_EQ(tab_strip->active_index(), 2);
  EXPECT_FALSE(tab_strip->group_model()
                   ->GetTabGroup(gid)
                   ->visual_data()
                   ->is_collapsed());

  // 2. Pin and mute 2 tabs ("Music Player" and "Design Doc") via direct JSON
  // fast-path.
  base::test::TestFuture<base::expected<base::Value, std::string>> pm_future;
  tools()->AnalyzeAndManageTabs(
      base::StringPrintf(
          R"({
            "facts": "Pinned and muted Music Player and Design Doc.",
            "actions": [
              {"type": "pin_tabs", "tab_ids": [%d, %d]},
              {"type": "mute_tabs", "tab_ids": [%d, %d]}
            ]
          })",
          music_tab_id, docs_tab_id, music_tab_id, docs_tab_id),
      pm_future.GetCallback());
  ASSERT_TRUE(pm_future.Get().has_value());

  // Pinned tabs move to indices 0 and 1.
  EXPECT_TRUE(tab_strip->GetTabAtIndex(0)->IsPinned());
  EXPECT_TRUE(tab_strip->GetWebContentsAt(0)->IsAudioMuted());
  EXPECT_TRUE(tab_strip->GetTabAtIndex(1)->IsPinned());
  EXPECT_TRUE(tab_strip->GetWebContentsAt(1)->IsAudioMuted());

  // 3. Undo the pin/mute batch and verify both tabs are unpinned, unmuted, and
  // back in their original positions (with Design Doc back in its group).
  base::test::TestFuture<base::expected<base::Value, std::string>> undo_future;
  tools()->AnalyzeAndManageTabs(
      R"({"facts": "Undid pin and mute.", "actions": [{"type": "undo_last_action"}]})",
      undo_future.GetCallback());
  ASSERT_TRUE(undo_future.Get().has_value());
  EXPECT_FALSE(tab_strip->GetTabAtIndex(1)->IsPinned());
  EXPECT_FALSE(tab_strip->GetWebContentsAt(1)->IsAudioMuted());
  EXPECT_FALSE(tab_strip->GetTabAtIndex(2)->IsPinned());
  EXPECT_FALSE(tab_strip->GetWebContentsAt(2)->IsAudioMuted());
  EXPECT_EQ(tab_strip->GetTabAtIndex(2)->GetGroup(), gid);
}

}  // namespace
}  // namespace ttc
