// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/accelerator_utils.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/startup/default_browser_prompt.h"
#include "chrome/browser/ui/startup/infobar_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/infobars/confirm_infobar.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/tracked_element_webcontents.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_main_parts.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/gfx/animation/animation_test_api.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTabContents);
constexpr char kInfoBarAcceptButton[] = "infobar_accept_button";
constexpr char kInfoBarDismissButton[] = "infobar_dismiss_button";
}  // namespace

class DefaultBrowserInfobarInteractiveTest : public InteractiveBrowserTest {
 public:
  ConfirmInfoBar* GetActiveInfoBar() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    infobars::ContentInfoBarManager* infobar_manager =
        infobars::ContentInfoBarManager::FromWebContents(web_contents);
    CHECK(infobar_manager);
    return static_cast<ConfirmInfoBar*>(infobar_manager->infobars()[0]);
  }

  auto NameAcceptButton() {
    return NameView(kInfoBarAcceptButton, base::BindLambdaForTesting([&]() {
                      return static_cast<views::View*>(
                          GetActiveInfoBar()->ok_button_for_testing());
                    }));
  }

  auto NameDismissButton() {
    return NameView(kInfoBarDismissButton, base::BindLambdaForTesting([&]() {
                      return static_cast<views::View*>(
                          GetActiveInfoBar()->dismiss_button_for_testing());
                    }));
  }
};

IN_PROC_BROWSER_TEST_F(DefaultBrowserInfobarInteractiveTest,
                       ShowsDefaultBrowserPrompt) {
  ShowPromptForTesting();
  RunTestSequence(
      WaitForShow(ConfirmInfoBar::kInfoBarElementId), FlushEvents(),
      AddInstrumentedTab(kSecondTabContents, GURL(chrome::kChromeUINewTabURL)),
      WaitForHide(ConfirmInfoBar::kInfoBarElementId));
}

class DefaultBrowserInfobarWithRefreshInteractiveTest
    : public DefaultBrowserInfobarInteractiveTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kDefaultBrowserPromptRefresh);

    DefaultBrowserInfobarInteractiveTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(DefaultBrowserInfobarWithRefreshInteractiveTest,
                       ShowsDefaultBrowserPromptOnNewTab) {
  ShowPromptForTesting();
  RunTestSequence(
      WaitForShow(ConfirmInfoBar::kInfoBarElementId), FlushEvents(),
      AddInstrumentedTab(kSecondTabContents, GURL(chrome::kChromeUINewTabURL)),
      WaitForShow(ConfirmInfoBar::kInfoBarElementId));
}

IN_PROC_BROWSER_TEST_F(DefaultBrowserInfobarWithRefreshInteractiveTest,
                       DoesNotShowDefaultBrowserPromptOnIncognitoTab) {
  ui::Accelerator incognito_accelerator;
  chrome::AcceleratorProviderForBrowser(browser())->GetAcceleratorForCommandId(
      IDC_NEW_INCOGNITO_WINDOW, &incognito_accelerator);

  ShowPromptForTesting();
  RunTestSequence(
      WaitForShow(ConfirmInfoBar::kInfoBarElementId), FlushEvents(),
      SendAccelerator(kBrowserViewElementId, incognito_accelerator),
      InAnyContext(
          WaitForShow(kBrowserViewElementId).SetTransitionOnlyOnEvent(true)),
      InSameContext(EnsureNotPresent(ConfirmInfoBar::kInfoBarElementId)));
}

IN_PROC_BROWSER_TEST_F(DefaultBrowserInfobarWithRefreshInteractiveTest,
                       RemovesAllBrowserPromptsOnAccept) {
  ShowPromptForTesting();
  RunTestSequence(
      WaitForShow(ConfirmInfoBar::kInfoBarElementId), FlushEvents(),
      AddInstrumentedTab(kSecondTabContents, GURL(chrome::kChromeUINewTabURL)),
      WaitForShow(ConfirmInfoBar::kInfoBarElementId), FlushEvents(),
      NameAcceptButton(), PressButton(kInfoBarAcceptButton), FlushEvents(),
      WaitForHide(ConfirmInfoBar::kInfoBarElementId), FlushEvents(),
      SelectTab(kTabStripElementId, 0), FlushEvents(),
      WaitForHide(ConfirmInfoBar::kInfoBarElementId));
}

IN_PROC_BROWSER_TEST_F(DefaultBrowserInfobarWithRefreshInteractiveTest,
                       HandlesAcceptWithDisabledAnimation) {
  // When animations are disabled, the info bar is destroyed sooner which can
  // cause UAF if not handled properly. This test ensures it is handled
  // properly.
  const gfx::AnimationTestApi::RenderModeResetter disable_rich_animations_ =
      gfx::AnimationTestApi::SetRichAnimationRenderMode(
          gfx::Animation::RichAnimationRenderMode::FORCE_DISABLED);
  ShowPromptForTesting();
  RunTestSequence(WaitForShow(ConfirmInfoBar::kInfoBarElementId), FlushEvents(),
                  NameAcceptButton(), PressButton(kInfoBarAcceptButton),
                  FlushEvents(),
                  WaitForHide(ConfirmInfoBar::kInfoBarElementId));
}

IN_PROC_BROWSER_TEST_F(DefaultBrowserInfobarWithRefreshInteractiveTest,
                       HandlesDismissWithDisabledAnimation) {
  // When animations are disabled, the info bar is destroyed sooner which can
  // cause UAF if not handled properly. This test ensures it is handled
  // properly.
  const gfx::AnimationTestApi::RenderModeResetter disable_rich_animations_ =
      gfx::AnimationTestApi::SetRichAnimationRenderMode(
          gfx::Animation::RichAnimationRenderMode::FORCE_DISABLED);
  ShowPromptForTesting();
  RunTestSequence(WaitForShow(ConfirmInfoBar::kInfoBarElementId), FlushEvents(),
                  NameDismissButton(), PressButton(kInfoBarDismissButton),
                  FlushEvents(),
                  WaitForHide(ConfirmInfoBar::kInfoBarElementId));
}

IN_PROC_BROWSER_TEST_F(DefaultBrowserInfobarWithRefreshInteractiveTest,
                       LogsMetrics) {
  base::HistogramTester histogram_tester;
  ShowPromptForTesting();
  RunTestSequence(WaitForShow(ConfirmInfoBar::kInfoBarElementId), FlushEvents(),
                  NameAcceptButton(), PressButton(kInfoBarAcceptButton),
                  FlushEvents(), WaitForHide(ConfirmInfoBar::kInfoBarElementId),
                  FlushEvents());

  histogram_tester.ExpectTotalCount(
      "DefaultBrowser.InfoBar.TimesShownBeforeAccept", 1);
}

IN_PROC_BROWSER_TEST_F(DefaultBrowserInfobarWithRefreshInteractiveTest,
                       DoesNotShowDismissedPromptOnNewWindows) {
  // Regression test for a bug where the DefaultBrowserPromptManager didn't
  // stop subscribing to TabStripModelObserver updates when new windows were
  // created.
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabMovedToNewWindowId);
  ShowPromptForTesting();
  RunTestSequence(
      // Open two tabs
      WaitForShow(ConfirmInfoBar::kInfoBarElementId), FlushEvents(),
      AddInstrumentedTab(kSecondTabContents, GURL(chrome::kChromeUINewTabURL)),
      WaitForShow(ConfirmInfoBar::kInfoBarElementId), FlushEvents(),
      // Dismiss prompt on one tab
      NameDismissButton(), PressButton(kInfoBarDismissButton), FlushEvents(),
      // Wait for hide
      WaitForHide(ConfirmInfoBar::kInfoBarElementId), FlushEvents(),
      // Move tab to new window
      InstrumentNextTab(kTabMovedToNewWindowId, AnyBrowser()),
      Do([&]() { chrome::MoveTabsToNewWindow(browser(), {1}); }),
      InAnyContext(WaitForWebContentsReady(kTabMovedToNewWindowId)),
      // Since the infobar isn't rendered synchronously, but the infobar is
      // created inside the manager, check the size of infobars for the moved
      // WebContents.
      InSameContext(CheckElement(
          kTabMovedToNewWindowId,
          [](ui::TrackedElement* el) {
            auto* const manager =
                infobars::ContentInfoBarManager::FromWebContents(
                    el->AsA<TrackedElementWebContents>()
                        ->owner()
                        ->web_contents());
            return manager->infobars().size();
          },
          0U)));
}
