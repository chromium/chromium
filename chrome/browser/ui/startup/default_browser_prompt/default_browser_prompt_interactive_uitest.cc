// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt_manager.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/app_menu_button.h"
#include "chrome/browser/ui/views/infobars/confirm_infobar.h"
#include "chrome/browser/ui/webui/test_support/webui_interactive_test_mixin.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/tracked_element_webcontents.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/switches.h"

#if !BUILDFLAG(IS_LINUX)
#include "chrome/browser/ui/chrome_pages.h"
#endif

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTabContents);
} // namespace

class DefaultBrowserPromptInteractiveTest
    : public WebUiInteractiveTestMixin<InteractiveBrowserTest> {
public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kDefaultBrowserPromptRefresh,
        {{features::kShowDefaultBrowserInfoBar.name, "true"},
         {features::kShowDefaultBrowserAppMenuChip.name, "true"},
         {features::kShowDefaultBrowserAppMenuItem.name, "true"}});

    shell_integration::DefaultBrowserWorker::DisableSetAsDefaultForTesting();
    InteractiveBrowserTest::SetUp();
  }

  static base::OnceCallback<bool(AppMenuButton *)>
  IsAppMenuChipDefaultBrowserPromptShowing(bool showing) {
    return base::BindOnce(
        [](bool showing, AppMenuButton *app_menu_button) {
          return showing == (app_menu_button->GetText() ==
                             l10n_util::GetStringUTF16(
                                 IDS_APP_MENU_BUTTON_DEFAULT_PROMPT));
        },
        showing);
  }

  InteractiveTestApi::MultiStep DoesAppMenuItemExist(bool exists) {
    return Steps(
        PressButton(kToolbarAppMenuButtonElementId),
        exists ? EnsurePresent(AppMenuModel::kSetBrowserAsDefaultMenuItem)
               : EnsureNotPresent(AppMenuModel::kSetBrowserAsDefaultMenuItem),
        WithView(kToolbarAppMenuButtonElementId,
                 [](AppMenuButton *app_menu_button) {
                   app_menu_button->CloseMenu();
                 }));
  }

  InteractiveTestApi::MultiStep
  RemovesAllBrowserDefaultPromptsWhen(InteractiveTestApi::MultiStep steps,
                                      bool preserve_app_menu_item = false) {
    return Steps(WaitForShow(ConfirmInfoBar::kInfoBarElementId),
                 WaitForShow(kToolbarAppMenuButtonElementId),
                 CheckView(kToolbarAppMenuButtonElementId,
                           IsAppMenuChipDefaultBrowserPromptShowing(true)),
                 DoesAppMenuItemExist(true),
                 AddInstrumentedTab(kSecondTabContents,
                                    GURL(chrome::kChromeUINewTabURL)),
                 WaitForShow(ConfirmInfoBar::kInfoBarElementId),
                 std::move(steps),
                 WaitForHide(ConfirmInfoBar::kInfoBarElementId),
                 SelectTab(kTabStripElementId, 0),
                 WaitForHide(ConfirmInfoBar::kInfoBarElementId),
                 CheckView(kToolbarAppMenuButtonElementId,
                           IsAppMenuChipDefaultBrowserPromptShowing(false)),
                 DoesAppMenuItemExist(preserve_app_menu_item));
  }

private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(DefaultBrowserPromptInteractiveTest,
                       RemovesAllBrowserDefaultPromptsOnAccept) {
  DefaultBrowserPromptManager::GetInstance()->MaybeShowPrompt();
  RunTestSequence(RemovesAllBrowserDefaultPromptsWhen(
      Steps(PressButton(ConfirmInfoBar::kOkButtonElementId))));
}

IN_PROC_BROWSER_TEST_F(DefaultBrowserPromptInteractiveTest,
                       RemovesAllBrowserDefaultPromptsOnDismiss) {
  DefaultBrowserPromptManager::GetInstance()->MaybeShowPrompt();
  RunTestSequence(RemovesAllBrowserDefaultPromptsWhen(
      Steps(PressButton(ConfirmInfoBar::kDismissButtonElementId)), true));
}

IN_PROC_BROWSER_TEST_F(DefaultBrowserPromptInteractiveTest,
                       RemovesAllBrowserDefaultPromptsOnAppMenuItemSelected) {
  DefaultBrowserPromptManager::GetInstance()->MaybeShowPrompt();
  RunTestSequence(RemovesAllBrowserDefaultPromptsWhen(
      Steps(PressButton(kToolbarAppMenuButtonElementId),
            SelectMenuItem(AppMenuModel::kSetBrowserAsDefaultMenuItem))));
}

// Linux test environment doesn't allow setting default via the
// chrome://settings/defaultBrowser page.
#if !BUILDFLAG(IS_LINUX)
IN_PROC_BROWSER_TEST_F(DefaultBrowserPromptInteractiveTest,
                       RemovesAllBrowserDefaultPromptsOnSettingsChange) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTabContents);
  const WebContentsInteractionTestUtil::DeepQuery kDefaultBrowserButton = {
      "settings-ui", "settings-main", "settings-basic-page",
      "settings-default-browser-page", "cr-button"};

  DefaultBrowserPromptManager::GetInstance()->MaybeShowPrompt();
  RunTestSequence(RemovesAllBrowserDefaultPromptsWhen(
      Steps(SelectTab(kTabStripElementId, 0), InstrumentTab(kFirstTabContents),
            NavigateWebContents(
                kFirstTabContents,
                GURL(chrome::GetSettingsUrl(chrome::kDefaultBrowserSubPage))),
            ClickElement(kFirstTabContents, kDefaultBrowserButton),
            SelectTab(kTabStripElementId, 1))));
}
#endif

class DefaultBrowserPromptInteractiveTestWithAppMenuDuration
    : public DefaultBrowserPromptInteractiveTest {
public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kDefaultBrowserPromptRefresh,
        {{features::kShowDefaultBrowserInfoBar.name, "true"},
         {features::kShowDefaultBrowserAppMenuChip.name, "true"},
         {features::kShowDefaultBrowserAppMenuItem.name, "true"},
         {features::kDefaultBrowserAppMenuDuration.name, "1s"}});

    InteractiveBrowserTest::SetUp();
  }

private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(DefaultBrowserPromptInteractiveTestWithAppMenuDuration,
                       RemovesAllBrowserDefaultPromptsOnAppMenuChipTimeout) {
  DefaultBrowserPromptManager::GetInstance()->MaybeShowPrompt();
  RunTestSequence(
      AddInstrumentedTab(kSecondTabContents, GURL(chrome::kChromeUINewTabURL)),
      WaitForHide(ConfirmInfoBar::kInfoBarElementId),
      SelectTab(kTabStripElementId, 0),
      WaitForHide(ConfirmInfoBar::kInfoBarElementId),
      CheckView(kToolbarAppMenuButtonElementId,
                IsAppMenuChipDefaultBrowserPromptShowing(false)),
      DoesAppMenuItemExist(true));
}

class DefaultBrowserPromptHeadlessBrowserTest
    : public DefaultBrowserPromptInteractiveTest {
 public:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(::switches::kHeadless);
    DefaultBrowserPromptInteractiveTest::SetUp();
  }
};

IN_PROC_BROWSER_TEST_F(DefaultBrowserPromptHeadlessBrowserTest, DoesNotCrash) {
  DefaultBrowserPromptManager::GetInstance()->MaybeShowPrompt();
  RunTestSequence(WaitForHide(ConfirmInfoBar::kInfoBarElementId));
}
