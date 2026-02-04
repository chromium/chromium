// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/scoped_test_mv2_enabler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/settings_overridden_dialog.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/search_engines/search_engines_test_util.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/extension_features.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace {

class SettingsOverriddenExplicitChoiceDialogInteractiveUiTest
    : public InteractiveBrowserTest {
 public:
  SettingsOverriddenExplicitChoiceDialogInteractiveUiTest() {
    feature_list_.InitAndEnableFeature(
        extensions_features::kSearchEngineExplicitChoiceDialog);
  }
  ~SettingsOverriddenExplicitChoiceDialogInteractiveUiTest() override = default;

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    search_test_utils::WaitForTemplateURLServiceToLoad(
        TemplateURLServiceFactory::GetForProfile(browser()->profile()));
  }

 protected:
  enum class DefaultSearch {
    kUseDefault,
    kUseNonGoogleFromDefaultList,
    kUseNewSearch,
  };

  auto SetNewSearchProvider(DefaultSearch search) {
    return Do([this, search]() {
      if (search == DefaultSearch::kUseDefault) {
        return;
      }

      TemplateURLService* const template_url_service =
          TemplateURLServiceFactory::GetForProfile(browser()->profile());

      bool new_search_shows_in_default_list = true;
      // If the test requires a search engine that doesn't show in the default
      // list, we need to add one.
      if (search == DefaultSearch::kUseNewSearch) {
        new_search_shows_in_default_list = false;
        template_url_service->Add(std::make_unique<TemplateURL>(
            *GenerateDummyTemplateURLData("test")));
      }

      TemplateURLService::TemplateURLVector template_urls =
          template_url_service->GetTemplateURLs();
      auto iter = std::ranges::find_if(
          template_urls,
          [template_url_service,
           new_search_shows_in_default_list](const TemplateURL* turl) {
            return !turl->HasGoogleBaseURLs(
                       template_url_service->search_terms_data()) &&
                   template_url_service->ShowInDefaultList(turl) ==
                       new_search_shows_in_default_list;
          });
      ASSERT_TRUE(iter != template_urls.end());

      template_url_service->SetUserSelectedDefaultSearchProvider(*iter);
    });
  }

  auto LoadExtensionOverridingSearch() {
    return Do([this]() {
      base::FilePath test_root_path;
      ASSERT_TRUE(
          base::PathService::Get(chrome::DIR_TEST_DATA, &test_root_path));

      Profile* const profile = browser()->profile();
      scoped_refptr<const extensions::Extension> extension =
          extensions::ChromeTestExtensionLoader(profile).LoadExtension(
              test_root_path.AppendASCII(
                  "extensions/search_provider_override"));
      ASSERT_TRUE(extension);
    });
  }

  auto PerformSearchFromOmnibox() {
    return Do([this]() {
      ui_test_utils::SendToOmniboxAndSubmit(browser(), "Penguin",
                                            base::TimeTicks::Now());
      content::WaitForLoadStop(
          browser()->tab_strip_model()->GetActiveWebContents());
    });
  }

  // Waits for the dialog to show, and verifies it's explicit-choice type.
  auto WaitForDialogToShow() {
    auto steps =
        Steps(WaitForShow(kSettingsOverriddenDialogSaveButtonId),
              EnsurePresent(kSettingsOverriddenDialogPreviousSettingButtonId),
              EnsurePresent(kSettingsOverriddenDialogNewSettingButtonId));
    AddDescriptionPrefix(steps, __func__);
    return steps;
  }

  auto ScreenshotDialog() {
    auto steps = Steps(
        // TODO(http://crbug.com/461806299): Add support for supplying the
        // dialog's ElementIdentifier via DialogModel, to eliminate the need for
        // Views-specific code here.
        NameViewRelative(kSettingsOverriddenDialogSaveButtonId, "Dialog",
                         [](views::View* save_button) {
                           return save_button->GetWidget()->GetRootView();
                         }),
        SetOnIncompatibleAction(
            OnIncompatibleAction::kIgnoreAndContinue,
            "Screenshot can only run in pixel_tests on Windows."),
        Screenshot("Dialog",
                   /*screenshot_name=*/std::string(),
                   /*baseline_cl=*/"7539783"));
    return steps;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  extensions::ScopedTestMV2Enabler mv2_enabler_;
};

IN_PROC_BROWSER_TEST_F(SettingsOverriddenExplicitChoiceDialogInteractiveUiTest,
                       SearchOverriddenDialog_PreviouslyGoogle) {
  RunTestSequence(SetNewSearchProvider(DefaultSearch::kUseDefault),
                  LoadExtensionOverridingSearch(), PerformSearchFromOmnibox(),
                  WaitForDialogToShow(), ScreenshotDialog());
}

IN_PROC_BROWSER_TEST_F(SettingsOverriddenExplicitChoiceDialogInteractiveUiTest,
                       SearchOverriddenDialog_PreviouslyNonGoogle) {
  RunTestSequence(
      SetNewSearchProvider(DefaultSearch::kUseNonGoogleFromDefaultList),
      LoadExtensionOverridingSearch(), PerformSearchFromOmnibox(),
      WaitForDialogToShow(), ScreenshotDialog());
}

IN_PROC_BROWSER_TEST_F(SettingsOverriddenExplicitChoiceDialogInteractiveUiTest,
                       SearchOverriddenDialog_PreviouslyOldExtension) {
  RunTestSequence(SetNewSearchProvider(DefaultSearch::kUseNewSearch),
                  LoadExtensionOverridingSearch(), PerformSearchFromOmnibox(),
                  WaitForDialogToShow(), ScreenshotDialog());
}

// TODO(http://crbug.com/461806299): Add tests for the following:
// - 'Escape' key won't dismiss the dialog
// - Clicking 'Save' without a selected options won't dismiss the dialog
// - Clicking 'Save' after selecting an option dismisses the dialog

}  // namespace
