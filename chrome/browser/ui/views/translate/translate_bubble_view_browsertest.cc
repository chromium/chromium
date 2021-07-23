// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/translate/translate_bubble_view.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/translate/translate_test_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/common/translate_switches.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interaction_sequence_views.h"
#include "ui/views/test/ax_event_counter.h"

namespace translate {

namespace {

static const char kTestValidScript[] =
    "var google = {};"
    "google.translate = (function() {"
    "  return {"
    "    TranslateService: function() {"
    "      return {"
    "        isAvailable : function() {"
    "          return true;"
    "        },"
    "        restore : function() {"
    "          return;"
    "        },"
    "        getDetectedLanguage : function() {"
    "          return \"\";"
    "        },"
    "        translatePage : function(sourceLang, targetLang,"
    "                                 onTranslateProgress) {"
    "          onTranslateProgress(100, true, false);"
    "        }"
    "      };"
    "    }"
    "  };"
    "})();"
    "cr.googleTranslate.onTranslateElementLoad();";

views::View* ElementToView(ui::TrackedElement* element) {
  return element->AsA<views::TrackedElementViews>()->view();
}
}  // namespace

class TranslateBubbleViewBrowserTest : public InProcessBrowserTest {
 public:
  TranslateBubbleViewBrowserTest() {}
  ~TranslateBubbleViewBrowserTest() override {}

  void SetUp() override {
    set_open_about_blank_on_browser_launch(true);
    TranslateManager::SetIgnoreMissingKeyForTesting(true);
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        switches::kTranslateScriptURL,
        embedded_test_server()->GetURL("/mock_translate_script.js").spec());
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("www.google.com", "127.0.0.1");
    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&TranslateBubbleViewBrowserTest::HandleRequest,
                            base::Unretained(this)));
    embedded_test_server()->StartAcceptingConnections();
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void WaitForPageTranslated(bool translated = true) {
    if (ChromeTranslateClient::FromWebContents(
            browser()->tab_strip_model()->GetActiveWebContents())
            ->GetLanguageState()
            .IsPageTranslated() != translated) {
      CreateTranslateWaiter(
          browser()->tab_strip_model()->GetActiveWebContents(),
          TranslateWaiter::WaitEvent::kIsPageTranslatedChanged)
          ->Wait();
    }
  }

 protected:
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    if (request.GetURL().path() != "/mock_translate_script.js")
      return nullptr;

    std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
        new net::test_server::BasicHttpResponse);
    http_response->set_code(net::HTTP_OK);
    http_response->set_content(kTestValidScript);
    http_response->set_content_type("text/javascript");
    return std::move(http_response);
  }

  void NavigateAndWaitForLanguageDetection(const GURL& url,
                                           const std::string& expected_lang) {
    ui_test_utils::NavigateToURL(browser(), url);

    while (expected_lang !=
           ChromeTranslateClient::FromWebContents(
               browser()->tab_strip_model()->GetActiveWebContents())
               ->GetLanguageState()
               .source_language()) {
      CreateTranslateWaiter(
          browser()->tab_strip_model()->GetActiveWebContents(),
          TranslateWaiter::WaitEvent::kLanguageDetermined)
          ->Wait();
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TranslateBubbleViewBrowserTest);
};

IN_PROC_BROWSER_TEST_F(TranslateBubbleViewBrowserTest,
                       CloseBrowserWithoutTranslating) {
  EXPECT_FALSE(TranslateBubbleView::GetCurrentBubble());

  // Show a French page and wait until the bubble is shown.
  GURL french_url = GURL(embedded_test_server()->GetURL("/french_page.html"));
  NavigateAndWaitForLanguageDetection(french_url, "fr");
  EXPECT_TRUE(TranslateBubbleView::GetCurrentBubble());

  // Close the window without translating. Spin the runloop to allow
  // asynchronous window closure to happen.
  chrome::CloseWindow(browser());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(TranslateBubbleView::GetCurrentBubble());
}

IN_PROC_BROWSER_TEST_F(TranslateBubbleViewBrowserTest,
                       CloseLastTabWithoutTranslating) {
  EXPECT_FALSE(TranslateBubbleView::GetCurrentBubble());

  // Show a French page and wait until the bubble is shown.
  GURL french_url = GURL(embedded_test_server()->GetURL("/french_page.html"));
  NavigateAndWaitForLanguageDetection(french_url, "fr");
  EXPECT_TRUE(TranslateBubbleView::GetCurrentBubble());

  // Close the tab without translating. Spin the runloop to allow asynchronous
  // window closure to happen.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  chrome::CloseWebContents(
      browser(), browser()->tab_strip_model()->GetActiveWebContents(), false);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(TranslateBubbleView::GetCurrentBubble());
}

IN_PROC_BROWSER_TEST_F(TranslateBubbleViewBrowserTest,
                       CloseAnotherTabWithoutTranslating) {
  EXPECT_FALSE(TranslateBubbleView::GetCurrentBubble());

  int active_index = browser()->tab_strip_model()->active_index();

  // Open another tab to load a French page on background.
  int french_index = active_index + 1;
  GURL french_url = GURL(embedded_test_server()->GetURL("/french_page.html"));
  chrome::AddTabAt(browser(), french_url, french_index, false);
  EXPECT_EQ(active_index, browser()->tab_strip_model()->active_index());
  EXPECT_EQ(2, browser()->tab_strip_model()->count());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(french_index);

  // The bubble is not shown because the tab is not activated.
  EXPECT_FALSE(TranslateBubbleView::GetCurrentBubble());

  // Close the French page tab immediately.
  chrome::CloseWebContents(browser(), web_contents, false);
  EXPECT_EQ(active_index, browser()->tab_strip_model()->active_index());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_FALSE(TranslateBubbleView::GetCurrentBubble());

  // Close the last tab.
  chrome::CloseWebContents(browser(),
                           browser()->tab_strip_model()->GetActiveWebContents(),
                           false);
}

IN_PROC_BROWSER_TEST_F(TranslateBubbleViewBrowserTest, AlertAccessibleEvent) {
  views::test::AXEventCounter counter(views::AXEventManager::Get());
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kAlert));

  GURL french_url = GURL(embedded_test_server()->GetURL("/french_page.html"));
  NavigateAndWaitForLanguageDetection(french_url, "fr");

  // TODO(crbug.com/1082217): This should produce one event instead of two.
  EXPECT_LT(0, counter.GetCount(ax::mojom::Event::kAlert));
}

// Verify that source language tab is selected and highlighted by
// default, and by selecting target language the page gets translated into
// target language and reverted to source language.
IN_PROC_BROWSER_TEST_F(TranslateBubbleViewBrowserTest, ClickLanguageTab) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);
  auto ElementClickCallback = base::BindRepeating(
      [](ui::TrackedElement* element, ui::ElementIdentifier element_id,
         ui::InteractionSequence::StepType step_type) {
        ui::AXActionData action_data;
        action_data.action = ax::mojom::Action::kDoDefault;
        views::View* view = ElementToView(element);
        view->HandleAccessibleAction(action_data);
      });

  // P1.Opened/Navigate to non english page > Hit on translate bubble icon.
  GURL french_url = GURL(embedded_test_server()->GetURL("/french_page.html"));
  NavigateAndWaitForLanguageDetection(french_url, "fr");

  base::RunLoop run_loop;
  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(run_loop.QuitClosure())
          .SetAbortedCallback(aborted.Get())
          // The dialog view of translate bubble is different across platforms.
          // On Linux/Mac it's a AlertDialog under BrowserRootView tree.
          // On Windows it's a separate LocationBarBubbleDelegateView.
          // That it's getting the root view of translate dialog via
          // GetCurrentBubble method.
          .AddStep(views::InteractionSequenceViews::WithInitialView(
              TranslateBubbleView::GetCurrentBubble()))
          // V1.Verify that by default the translate bubble’s source language
          // tab is selected and highlighted.
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetElementID(TranslateBubbleView::kSourceLanguageTab)
                       .SetStartCallback(base::BindOnce(
                           [](ui::TrackedElement* element,
                              ui::ElementIdentifier element_id,
                              ui::InteractionSequence::StepType step_type) {
                             auto* source_tab = static_cast<views::Tab*>(
                                 ElementToView(element));
                             EXPECT_TRUE(source_tab->selected());
                           }))
                       .Build())
          // P2.To translate the page,tap the target language tab.
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetElementID(TranslateBubbleView::kTargetLanguageTab)
                       .SetStartCallback(ElementClickCallback)
                       .Build())
          // V2.Verify that once the page is translated, the target language tab
          // will be selected.
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetElementID(TranslateBubbleView::kTargetLanguageTab)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [this](ui::TrackedElement* element,
                                  ui::ElementIdentifier element_id,
                                  ui::InteractionSequence::StepType step_type) {
                             WaitForPageTranslated(true);
                             auto* target_tab = static_cast<views::Tab*>(
                                 ElementToView(element));
                             EXPECT_TRUE(target_tab->selected());
                           }))
                       .Build())
          // P3.To translate the page to source language again, tapping the
          // source language.
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetElementID(TranslateBubbleView::kSourceLanguageTab)
                       .SetStartCallback(ElementClickCallback)
                       .Build())
          // V3.Verify that page reverts the translation should shows in
          // original content.
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetElementID(TranslateBubbleView::kSourceLanguageTab)
                       .SetMustRemainVisible(false)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [this](ui::TrackedElement* element,
                                  ui::ElementIdentifier element_id,
                                  ui::InteractionSequence::StepType step_type) {
                             WaitForPageTranslated(false);
                             auto* source_tab = static_cast<views::Tab*>(
                                 ElementToView(element));
                             EXPECT_TRUE(source_tab->selected());
                           }))
                       .Build())
          // P4.Tap on cancel button option in the translate bubble popup box.
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetElementID(TranslateBubbleView::kCloseButton)
                       .SetStartCallback(ElementClickCallback)
                       .SetMustRemainVisible(false)
                       .Build())
          // V4.Tapping the close button dismisses the translate bubble.
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetElementID(TranslateBubbleView::kIdentifier)
                       .SetType(ui::InteractionSequence::StepType::kHidden)
                       .Build())
          .Build();

  sequence->Start();
  run_loop.Run();
}

}  // namespace translate
