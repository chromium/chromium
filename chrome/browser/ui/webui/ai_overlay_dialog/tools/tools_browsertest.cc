// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ai_overlay_dialog/tools/tools.h"

#include <memory>
#include <string>

#include "base/hash/hash.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/translate/translate_test_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/ai_overlay_dialog/ai_overlay_dialog_page_handler.h"
#include "chrome/browser/ui/webui/ai_overlay_dialog/page_context_monitor.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/common/translate_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ttc {
namespace {

using ScrollResult = base::expected<std::monostate, std::string>;
using PerformSearchResult = base::expected<std::monostate, std::string>;
using FindAndHighlightResult = base::expected<std::monostate, std::string>;
using PlayVideoResult = base::expected<std::monostate, std::string>;
using PauseVideoResult = base::expected<std::monostate, std::string>;
using SeekToTimestampResult = base::expected<std::monostate, std::string>;
using OpenUrlResult = base::expected<std::monostate, std::string>;
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
  void UpdateCurrentPageContext(const std::string& page_title,
                                const std::string& page_content) override {}
  void SetCaptionsVisible(bool visible) override {}
  void SetUsePersona(bool use_persona) override {}

 private:
  mojo::Receiver<ai_overlay_dialog::mojom::Page> receiver_{this};
};

class AiOverlayToolsBrowserTest : public InProcessBrowserTest {
 public:
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
        std::make_unique<PageContextMonitor>(*browser(), *page_handler_);

    mojo::PendingRemote<ai_overlay_dialog::mojom::AiOverlayTools> remote;
    tools_ = std::make_unique<AiOverlayTools>(
        remote.InitWithNewPipeAndPassReceiver(), browser(),
        page_context_monitor_.get());
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
        browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    content::WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_TRUE(content::ExecJs(contents, "document.title = '" + title + "'"));
  }

 private:
  FakePage fake_page_;
  std::unique_ptr<AiOverlayDialogPageHandler> page_handler_;
  std::unique_ptr<PageContextMonitor> page_context_monitor_;
  std::unique_ptr<AiOverlayTools> tools_;
};

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, OpenUrlNewTab) {
  GURL initial_url = embedded_test_server()->GetURL("/empty.html?initial");
  GURL target_url = embedded_test_server()->GetURL("/empty.html?target");

  AddTabWithTitle(initial_url, "Initial");
  int initial_count = browser()->tab_strip_model()->count();

  base::test::TestFuture<OpenUrlResult> future;
  tools()->OpenUrl(target_url.spec(), /*new_tab=*/true, future.GetCallback());

  EXPECT_TRUE(future.Get().has_value());
  EXPECT_EQ(initial_count + 1, browser()->tab_strip_model()->count());
  EXPECT_EQ(
      target_url,
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, OpenUrlCurrentTab) {
  GURL initial_url = embedded_test_server()->GetURL("/empty.html?initial");
  GURL target_url = embedded_test_server()->GetURL("/empty.html?target");

  AddTabWithTitle(initial_url, "Initial");
  int initial_count = browser()->tab_strip_model()->count();

  base::test::TestFuture<OpenUrlResult> future;
  tools()->OpenUrl(target_url.spec(), /*new_tab=*/false, future.GetCallback());

  EXPECT_TRUE(future.Get().has_value());
  EXPECT_EQ(initial_count, browser()->tab_strip_model()->count());
  EXPECT_EQ(
      target_url,
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, OpenUrlInvalid) {
  base::test::TestFuture<OpenUrlResult> future;
  tools()->OpenUrl("invalid_url", /*new_tab=*/true, future.GetCallback());

  EXPECT_FALSE(future.Get().has_value());
  EXPECT_EQ("Invalid URL", future.Get().error());
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, SwitchTabByTitle) {
  AddTabWithTitle(embedded_test_server()->GetURL("/empty.html?1"), "First Tab");
  AddTabWithTitle(embedded_test_server()->GetURL("/empty.html?2"),
                  "Second Tab");
  AddTabWithTitle(embedded_test_server()->GetURL("/empty.html?3"), "Third Tab");

  int initial_active = browser()->tab_strip_model()->active_index();

  base::test::TestFuture<SwitchTabResult> future;
  tools()->SwitchTab("second", future.GetCallback());

  EXPECT_TRUE(future.Get().has_value());
  EXPECT_EQ(initial_active - 1, browser()->tab_strip_model()->active_index());
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, SwitchTabByUrl) {
  AddTabWithTitle(embedded_test_server()->GetURL("/empty.html?1"), "First Tab");
  GURL target_url = embedded_test_server()->GetURL("/empty.html?target_path");
  AddTabWithTitle(target_url, "Second Tab");
  AddTabWithTitle(embedded_test_server()->GetURL("/empty.html?3"), "Third Tab");

  int initial_active = browser()->tab_strip_model()->active_index();

  base::test::TestFuture<SwitchTabResult> future;
  tools()->SwitchTab("target_path", future.GetCallback());

  EXPECT_TRUE(future.Get().has_value());
  EXPECT_EQ(target_url, future.Get().value()->url);
  EXPECT_EQ(initial_active - 1, browser()->tab_strip_model()->active_index());
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

  int initial_count = browser()->tab_strip_model()->count();

  base::test::TestFuture<CloseTabResult> future;
  tools()->CloseCurrentTab(future.GetCallback());

  EXPECT_TRUE(future.Get().has_value());
  EXPECT_EQ(initial_count - 1, browser()->tab_strip_model()->count());
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, GoBack) {
  GURL first_url = embedded_test_server()->GetURL("/empty.html?1");
  GURL second_url = embedded_test_server()->GetURL("/empty.html?2");

  AddTabWithTitle(first_url, "First");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), second_url));

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::NavigationController& controller = contents->GetController();

  EXPECT_TRUE(controller.CanGoBack());

  base::test::TestFuture<GoBackResult> future;
  tools()->GoBack(future.GetCallback());

  EXPECT_TRUE(future.Get().has_value());
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, GoBackCannotGoBack) {
  AddTabWithTitle(embedded_test_server()->GetURL("/empty.html?1"), "First");
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
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
      browser()->tab_strip_model()->GetActiveWebContents();
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
      browser()->tab_strip_model()->GetActiveWebContents();
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

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, Scroll) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/empty.html")));

  base::test::TestFuture<ScrollResult> future;
  tools()->Scroll(ai_overlay_dialog::mojom::ScrollGranularity::kPage, 1.0,
                  future.GetCallback());

  EXPECT_TRUE(future.Get().has_value());
}

// For media tools, we utilize the embedded test server's built-in
// video-with-metadata.html page to test active media sessions.

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, FindAndHighlightSuccess) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("data:text/html,<html><body><p>This is a test paragraph "
                      "that we want to find.</p></body></html>")));

  base::test::TestFuture<FindAndHighlightResult> future;
  tools()->FindAndHighlight("test paragraph", future.GetCallback());

  EXPECT_TRUE(future.Get().has_value());
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, FindAndHighlightNotFound) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("data:text/html,<html><body><p>This is a test paragraph "
                      "that we want to find.</p></body></html>")));

  base::test::TestFuture<FindAndHighlightResult> future;
  tools()->FindAndHighlight("missing text", future.GetCallback());

  EXPECT_FALSE(future.Get().has_value());
  EXPECT_EQ("No match found", future.Get().error());
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
      browser()->tab_strip_model()->GetActiveWebContents();

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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/empty.html")));

  base::test::TestFuture<TranslatePageResult> future;
  tools()->TranslatePage("", future.GetCallback());

  EXPECT_TRUE(future.Get().has_value());

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/empty.html")));

  base::test::TestFuture<TranslatePageResult> future;
  tools()->TranslatePage("fr", future.GetCallback());

  EXPECT_TRUE(future.Get().has_value());

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

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
                            ->tab_strip_model()
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
                            ->tab_strip_model()
                            ->GetActiveWebContents()
                            ->GetLastCommittedURL());
}

}  // namespace
}  // namespace ttc
