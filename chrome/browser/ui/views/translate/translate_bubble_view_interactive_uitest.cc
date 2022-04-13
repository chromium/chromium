// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/translate/translate_bubble_view.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/i18n/base_i18n_switches.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/translate/translate_test_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/common/translate_switches.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/network_connection_change_simulator.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interaction_sequence_views.h"
#include "ui/views/test/button_test_api.h"

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

void ElementClickCallback(ui::InteractionSequence* sequence,
                          ui::TrackedElement* element) {
  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kDoDefault;
  views::View* view = ElementToView(element);
  view->HandleAccessibleAction(action_data);
}

void ButtonClickCallBack(ui::InteractionSequence* sequence,
                         ui::TrackedElement* element) {
  // The button might be ignored by HandleAccessibleAction() because it has a
  // bound of size 0 (not yet laid out). Hence, notify click directly.
  views::test::ButtonTestApi(
      static_cast<views::Button*>(ElementToView(element)))
      .NotifyClick(ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(),
                                  gfx::Point(), ui::EventTimeForNow(), 0, 0));
}

}  // namespace

class TranslateBubbleViewUITest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<std::string> {
 public:
  TranslateBubbleViewUITest() = default;
  ~TranslateBubbleViewUITest() override = default;
  explicit TranslateBubbleViewUITest(const TranslateBubbleUiEvent&) = delete;
  TranslateBubbleUiEvent& operator=(const TranslateBubbleUiEvent&) = delete;

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
    if (GetParam() == "RightToLeft") {
      command_line->AppendSwitchASCII(::switches::kForceUIDirection,
                                      ::switches::kForceDirectionRTL);
      command_line->AppendSwitchASCII(::switches::kForceTextDirection,
                                      ::switches::kForceDirectionRTL);
    } else if (GetParam() == "Incognito") {
      command_line->AppendSwitch(::switches::kIncognito);
    } else if (GetParam() == "Theme") {
      command_line->AppendSwitchASCII(::switches::kInstallAutogeneratedTheme,
                                      "121,0,0");
    }
    command_line->AppendSwitch(::switches::kOverrideLanguageDetection);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("www.google.com", "127.0.0.1");
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &TranslateBubbleViewUITest::HandleRequest, base::Unretained(this)));
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
          translated ? TranslateWaiter::WaitEvent::kPageTranslated
                     : TranslateWaiter::WaitEvent::kIsPageTranslatedChanged)
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
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    if (GetParam() == "MultipleBubble") {
      chrome::GenerateQRCodeFromPageAction(browser());
    }

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
};

// Verify that source language tab is selected and highlighted by
// default, and by selecting target language the page gets translated into
// target language and reverted to source language.
IN_PROC_BROWSER_TEST_P(TranslateBubbleViewUITest, ClickLanguageTab) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  // P1.Opened/Navigate to non english page > Hit on translate bubble icon.
  GURL french_url = GURL(embedded_test_server()->GetURL("/french_page.html"));
  NavigateAndWaitForLanguageDetection(french_url, "fr");

  ui::InteractionSequence::Builder()
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
                   .SetStartCallback(
                       base::BindOnce([](ui::InteractionSequence*,
                                         ui::TrackedElement* element) {
                         auto* source_tab =
                             static_cast<views::Tab*>(ElementToView(element));
                         EXPECT_TRUE(source_tab->selected());
                       }))
                   .Build())
      // P2.To translate the page,tap the target language tab.
      .AddStep(ui::InteractionSequence::StepBuilder()
                   .SetElementID(TranslateBubbleView::kTargetLanguageTab)
                   .SetStartCallback(base::BindOnce(ElementClickCallback))
                   .Build())
      // V2.Verify that once the page is translated, the target language tab
      // will be selected.
      .AddStep(ui::InteractionSequence::StepBuilder()
                   .SetElementID(TranslateBubbleView::kTargetLanguageTab)
                   .SetStartCallback(base::BindLambdaForTesting(
                       [this](ui::InteractionSequence*,
                              ui::TrackedElement* element) {
                         WaitForPageTranslated(true);
                         auto* target_tab =
                             static_cast<views::Tab*>(ElementToView(element));
                         EXPECT_TRUE(target_tab->selected());
                       }))
                   .Build())
      // P3.To translate the page to source language again, tapping the
      // source language.
      .AddStep(ui::InteractionSequence::StepBuilder()
                   .SetElementID(TranslateBubbleView::kSourceLanguageTab)
                   .SetStartCallback(base::BindOnce(ElementClickCallback))
                   .Build())
      // V3.Verify that page reverts the translation should shows in
      // original content.
      .AddStep(ui::InteractionSequence::StepBuilder()
                   .SetElementID(TranslateBubbleView::kSourceLanguageTab)
                   .SetMustRemainVisible(false)
                   .SetStartCallback(base::BindLambdaForTesting(
                       [this](ui::InteractionSequence*,
                              ui::TrackedElement* element) {
                         WaitForPageTranslated(false);
                         auto* source_tab =
                             static_cast<views::Tab*>(ElementToView(element));
                         EXPECT_TRUE(source_tab->selected());
                       }))
                   .Build())
      // P4.Tap on cancel button option in the translate bubble popup box.
      .AddStep(ui::InteractionSequence::StepBuilder()
                   .SetElementID(TranslateBubbleView::kCloseButton)
                   .SetStartCallback(base::BindOnce(ButtonClickCallBack))
                   .SetMustRemainVisible(false)
                   .Build())
      // V4.Tapping the close button dismisses the translate bubble.
      .AddStep(ui::InteractionSequence::StepBuilder()
                   .SetElementID(TranslateBubbleView::kIdentifier)
                   .SetType(ui::InteractionSequence::StepType::kHidden)
                   .Build())
      .Build()
      ->RunSynchronouslyForTesting();
}

// Verify the "Choose another language" option from 3 dot menu.
IN_PROC_BROWSER_TEST_P(TranslateBubbleViewUITest, ChooseAnotherLanguage) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  // P1. Opened/Navigate to non english page.
  GURL french_url = GURL(embedded_test_server()->GetURL("/french_page.html"));
  NavigateAndWaitForLanguageDetection(french_url, "fr");

  ui::InteractionSequence::Builder()
      .SetAbortedCallback(aborted.Get())
      .AddStep(views::InteractionSequenceViews::WithInitialView(
          TranslateBubbleView::GetCurrentBubble()))
      // P2. Click on translate bubble > Click on 3 dot menu.
      .AddStep(ui::InteractionSequence::StepBuilder()
                   .SetElementID(TranslateBubbleView::kOptionsMenuButton)
                   .SetStartCallback(base::BindOnce(ElementClickCallback))
                   .Build())
      // P3. Click on the “Choose another language” option.
      .AddStep(ui::InteractionSequence::StepBuilder()
                   .SetElementID(TranslateBubbleView::kChangeTargetLanguage)
                   .SetStartCallback(base::BindOnce(ElementClickCallback))
                   .SetMustRemainVisible(false)
                   .Build())
      // V1. Verify that this dismisses the options menu and brings up a new
      // bubble with a combobox that populates a list of all available
      // languages.
      // Note: DCHECK(!processing_step_) in
      // ui::InteractionSequence::DoStepTransition() will fail when adding a
      // kHidden step for kChangeTargetLanguage.
      .AddStep(ui::InteractionSequence::StepBuilder()
                   .SetElementID(TranslateBubbleView::kTargetLanguageCombobox)
                   .SetType(ui::InteractionSequence::StepType::kShown)
                   .Build())
      .AddStep(ui::InteractionSequence::StepBuilder()
                   .SetElementID(TranslateBubbleView::kChangeTargetLanguage)
                   .SetType(ui::InteractionSequence::StepType::kHidden)
                   .Build())
      // P4. Select a language from the list and select translate.
      .AddStep(
          ui::InteractionSequence::StepBuilder()
              .SetElementID(TranslateBubbleView::kTargetLanguageCombobox)
              .SetStartCallback(base::BindLambdaForTesting(
                  [&](ui::InteractionSequence*, ui::TrackedElement* element) {
                    auto* advanced_view_target =
                        static_cast<views::Combobox*>(ElementToView(element));
                    advanced_view_target->SetSelectedRow(0);
                  }))
              .Build())
      .AddStep(ui::InteractionSequence::StepBuilder()
                   .SetElementID(TranslateBubbleView::kTargetLanguageDoneButton)
                   .SetStartCallback(base::BindOnce(ButtonClickCallBack))
                   .SetMustRemainVisible(false)
                   .Build())
      // V2. Verify that the language list will be dismissed, the target
      // language tab shows updated target language. Source language tab is
      // no longer highlighted and the target language tab will be
      // highlighted once translation is completed.
      .AddStep(ui::InteractionSequence::StepBuilder()
                   .SetElementID(TranslateBubbleView::kTargetLanguageCombobox)
                   .SetType(ui::InteractionSequence::StepType::kHidden)
                   .Build())
      .AddStep(ui::InteractionSequence::StepBuilder()
                   .SetElementID(TranslateBubbleView::kTargetLanguageTab)
                   .SetStartCallback(base::BindLambdaForTesting(
                       [&](ui::InteractionSequence*,
                           ui::TrackedElement* element) {
                         WaitForPageTranslated(true);
                         auto* target_tab =
                             static_cast<views::Tab*>(ElementToView(element));
                         EXPECT_EQ(target_tab->GetTitleText(),
                                   TranslateBubbleView::GetCurrentBubble()
                                       ->model()
                                       ->GetTargetLanguageNameAt(0));
                         EXPECT_TRUE(target_tab->selected());
                       }))
                   .Build())
      // P5. Select revert.
      .AddStep(ui::InteractionSequence::StepBuilder()
                   .SetElementID(TranslateBubbleView::kSourceLanguageTab)
                   .SetStartCallback(base::BindOnce(ElementClickCallback))
                   .Build())
      // V3. Verify that the page should revert to original language and source
      // language tab is selected.
      .AddStep(ui::InteractionSequence::StepBuilder()
                   .SetElementID(TranslateBubbleView::kSourceLanguageTab)
                   .SetStartCallback(base::BindLambdaForTesting(
                       [this](ui::InteractionSequence*,
                              ui::TrackedElement* element) {
                         WaitForPageTranslated(false);
                         auto* source_tab =
                             static_cast<views::Tab*>(ElementToView(element));
                         EXPECT_TRUE(source_tab->selected());
                       }))
                   .Build())
      .Build()
      ->RunSynchronouslyForTesting();
}

// Verify the "Page is not in (source language)" option from 3 dot menu.
IN_PROC_BROWSER_TEST_P(TranslateBubbleViewUITest,
                       ClickPageNotInSourceLanguage) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  // P1. Opened/Navigate to non english page.
  GURL french_url = GURL(embedded_test_server()->GetURL("/french_page.html"));
  NavigateAndWaitForLanguageDetection(french_url, "fr");

  ui::InteractionSequence::Builder()
      .SetAbortedCallback(aborted.Get())
      .AddStep(views::InteractionSequenceViews::WithInitialView(
          TranslateBubbleView::GetCurrentBubble()))
      // P2. Click on translate bubble > Click on 3 dot menu.
      .AddStep(ui::InteractionSequence::StepBuilder()
                   .SetElementID(TranslateBubbleView::kOptionsMenuButton)
                   .SetStartCallback(base::BindOnce(ElementClickCallback))
                   .Build())
      // P3. Click on the “Page is not in {source languages}?” option.
      .AddStep(ui::InteractionSequence::StepBuilder()
                   .SetElementID(TranslateBubbleView::kChangeSourceLanguage)
                   .SetStartCallback(base::BindOnce(ElementClickCallback))
                   .SetMustRemainVisible(false)
                   .Build())
      // V1. Verify that this dismisses the options menu and brings up a new
      // bubble with a combobox that populates a list of all available
      // languages.
      .AddStep(ui::InteractionSequence::StepBuilder()
                   .SetElementID(TranslateBubbleView::kSourceLanguageCombobox)
                   .SetType(ui::InteractionSequence::StepType::kShown)
                   .Build())
      .AddStep(ui::InteractionSequence::StepBuilder()
                   .SetElementID(TranslateBubbleView::kChangeSourceLanguage)
                   .SetType(ui::InteractionSequence::StepType::kHidden)
                   .Build())
      // P4. Select a language from the list and select translate.
      .AddStep(ui::InteractionSequence::StepBuilder()
                   .SetElementID(TranslateBubbleView::kSourceLanguageCombobox)
                   .SetStartCallback(base::BindLambdaForTesting(
                       [&](ui::InteractionSequence*,
                           ui::TrackedElement* element) {
                         auto* advanced_view_source =
                             static_cast<views::Combobox*>(
                                 ElementToView(element));
                         advanced_view_source->SetSelectedRow(
                             1);  // 0 = Detected Language
                       }))
                   .Build())
      .AddStep(ui::InteractionSequence::StepBuilder()
                   .SetElementID(TranslateBubbleView::kSourceLanguageDoneButton)
                   .SetStartCallback(base::BindOnce(ButtonClickCallBack))
                   .SetMustRemainVisible(false)
                   .Build())
      // V2. The language list will be dismissed, the source language tab
      // shows updated source language. Source language tab is no longer
      // highlighted and the target language tab will be highlighted once
      // the translation is completed.
      .AddStep(ui::InteractionSequence::StepBuilder()
                   .SetElementID(TranslateBubbleView::kSourceLanguageCombobox)
                   .SetType(ui::InteractionSequence::StepType::kHidden)
                   .Build())
      .AddStep(
          ui::InteractionSequence::StepBuilder()
              .SetElementID(TranslateBubbleView::kSourceLanguageTab)
              .SetType(ui::InteractionSequence::StepType::kShown)
              .SetStartCallback(base::BindLambdaForTesting(
                  [&](ui::InteractionSequence*, ui::TrackedElement* element) {
                    WaitForPageTranslated(true);
                    auto* source_tab =
                        static_cast<views::Tab*>(ElementToView(element));
                    EXPECT_EQ(source_tab->GetTitleText(),
                              TranslateBubbleView::GetCurrentBubble()
                                  ->model()
                                  ->GetSourceLanguageNameAt(1));
                    EXPECT_TRUE(!source_tab->selected());

                    auto* target_tab = static_cast<views::Tab*>(ElementToView(
                        ui::ElementTracker::GetElementTracker()
                            ->GetFirstMatchingElement(
                                TranslateBubbleView::kTargetLanguageTab,
                                element->context())));
                    EXPECT_TRUE(target_tab->selected());
                  }))
              .Build())
      // P5. Select revert.
      // Note: The revert means revert the page to its original language,
      // but the source tab are still showing the source language we
      // selected in P4. See https://crbug.com/1222050.
      .AddStep(ui::InteractionSequence::StepBuilder()
                   .SetElementID(TranslateBubbleView::kSourceLanguageTab)
                   .SetStartCallback(base::BindOnce(ElementClickCallback))
                   .Build())
      // V3. Verify that the page should revert to original language and source
      // language tab is selected.
      .AddStep(ui::InteractionSequence::StepBuilder()
                   .SetElementID(TranslateBubbleView::kSourceLanguageTab)
                   .SetStartCallback(base::BindLambdaForTesting(
                       [this](ui::InteractionSequence*,
                              ui::TrackedElement* element) {
                         WaitForPageTranslated(false);
                         auto* source_tab =
                             static_cast<views::Tab*>(ElementToView(element));
                         EXPECT_TRUE(source_tab->selected());
                       }))
                   .Build())
      .Build()
      ->RunSynchronouslyForTesting();
}

// Verify the error handling OR network interruption.
IN_PROC_BROWSER_TEST_P(TranslateBubbleViewUITest, NetworkInterruption) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  bool offline = false;
  content::URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](content::URLLoaderInterceptor::RequestParams* params) -> bool {
        if (!offline)
          return false;
        params->client->OnComplete(
            network::URLLoaderCompletionStatus(net::ERR_INTERNET_DISCONNECTED));
        return true;
      }));

  // Changing the URLLoaderFactory used by translate fetcher to the one for
  // BrowserProcess. The original one is owned by SystemNetworkContextManager
  // which cannot intercepted by content::URLLoaderInterceptor.
  TranslateDownloadManager::GetInstance()->set_url_loader_factory(
      browser()
          ->profile()
          ->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess());

  // P1. Opened/Navigate to non english page > Hit on translate bubble icon.
  GURL french_url = GURL(embedded_test_server()->GetURL("/french_page.html"));
  NavigateAndWaitForLanguageDetection(french_url, "fr");

  ui::InteractionSequence::Builder()
      .SetAbortedCallback(aborted.Get())
      .AddStep(views::InteractionSequenceViews::WithInitialView(
          TranslateBubbleView::GetCurrentBubble()))
      // P2. Tap the target language tab.
      .AddStep(ui::InteractionSequence::StepBuilder()
                   .SetElementID(TranslateBubbleView::kTargetLanguageTab)
                   .SetStartCallback(base::BindOnce(ElementClickCallback))
                   .Build())
      // V1. Wait until the translation is completed.
      .AddStep(ui::InteractionSequence::StepBuilder()
                   .SetElementID(TranslateBubbleView::kTargetLanguageTab)
                   .SetStartCallback(base::BindLambdaForTesting(
                       [&](ui::InteractionSequence*,
                           ui::TrackedElement* element) {
                         WaitForPageTranslated(true);
                         auto* target_tab =
                             static_cast<views::Tab*>(ElementToView(element));
                         EXPECT_TRUE(target_tab->selected());

                         // P3.Turn off the network.
                         offline = true;
                         // Clear the script cache.
                         TranslateDownloadManager::GetInstance()
                             ->ClearTranslateScriptForTesting();
                       }))
                   .Build())
      // P4. Click on the source language tab.
      .AddStep(ui::InteractionSequence::StepBuilder()
                   .SetElementID(TranslateBubbleView::kSourceLanguageTab)
                   .SetStartCallback(base::BindOnce(ElementClickCallback))
                   .Build())
      // V3. The page should revert to the original language.
      .AddStep(ui::InteractionSequence::StepBuilder()
                   .SetElementID(TranslateBubbleView::kSourceLanguageTab)
                   .SetMustRemainVisible(false)
                   .SetStartCallback(base::BindLambdaForTesting(
                       [this](ui::InteractionSequence*,
                              ui::TrackedElement* element) {
                         WaitForPageTranslated(false);
                         auto* source_tab =
                             static_cast<views::Tab*>(ElementToView(element));
                         EXPECT_TRUE(source_tab->selected());
                       }))
                   .Build())
      // P4. Click on the target language tab again.
      .AddStep(ui::InteractionSequence::StepBuilder()
                   .SetElementID(TranslateBubbleView::kTargetLanguageTab)
                   .SetStartCallback(base::BindOnce(ElementClickCallback))
                   .SetMustRemainVisible(false)
                   .Build())
      // V4. Translate bubble is dismissed, An error bubble will be shown
      // with a message saying "This page could not be translated.".
      .AddStep(
          ui::InteractionSequence::StepBuilder()
              .SetElementID(TranslateBubbleView::kErrorMessage)
              .SetType(ui::InteractionSequence::StepType::kShown)
              .SetStartCallback(base::BindOnce(
                  [](ui::InteractionSequence*, ui::TrackedElement* element) {
                    auto* error_message_label =
                        static_cast<views::Label*>(ElementToView(element));
                    EXPECT_EQ(
                        error_message_label->GetText(),
                        l10n_util::GetStringUTF16(
                            IDS_TRANSLATE_BUBBLE_COULD_NOT_TRANSLATE_TITLE));
                  }))
              .Build())
      .AddStep(ui::InteractionSequence::StepBuilder()
                   .SetElementID(TranslateBubbleView::kChangeTargetLanguage)
                   .SetType(ui::InteractionSequence::StepType::kHidden)
                   .Build())
      .Build()
      ->RunSynchronouslyForTesting();
}

INSTANTIATE_TEST_SUITE_P(All,
                         TranslateBubbleViewUITest,
                         ::testing::Values("Default",
                                           "RightToLeft",
                                           "Incognito",
                                           "MultipleBubble",
                                           "Theme"),
                         [](const ::testing::TestParamInfo<std::string>& inf) {
                           return inf.param;
                         });

}  // namespace translate
