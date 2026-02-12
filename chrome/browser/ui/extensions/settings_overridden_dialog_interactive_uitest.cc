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
#include "chrome/browser/image_fetcher/image_fetcher_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/settings_overridden_dialog.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/extensions/rich_radio_button.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/image_fetcher/core/image_fetcher_service.h"
#include "components/image_fetcher/core/mock_image_fetcher.h"
#include "components/image_fetcher/core/request_metadata.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/search_engines/search_engines_test_util.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/extension_features.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace {

using testing::_;

// This makes icon fetching testable on the dialog, without resorting to
// fallback/placeholder icons.
class MockImageFetcherService : public image_fetcher::ImageFetcherService {
 public:
  MockImageFetcherService() = default;
  ~MockImageFetcherService() override = default;

  image_fetcher::MockImageFetcher* mock_image_fetcher() {
    return &mock_image_fetcher_;
  }

  // image_fetcher::ImageFetcherService:
  image_fetcher::ImageFetcher* GetImageFetcher(
      image_fetcher::ImageFetcherConfig config) override {
    return &mock_image_fetcher_;
  }

 private:
  image_fetcher::MockImageFetcher mock_image_fetcher_;
};

class SettingsOverriddenExplicitChoiceDialogInteractiveUiTest
    : public InteractiveBrowserTest {
 public:
  SettingsOverriddenExplicitChoiceDialogInteractiveUiTest() {
    feature_list_.InitAndEnableFeature(
        extensions_features::kSearchEngineExplicitChoiceDialog);

    // Register a mock image-fetcher factory service so that it's created
    // in place of the default image-fetcher service.
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &SettingsOverriddenExplicitChoiceDialogInteractiveUiTest::
                    OnWillCreateBrowserContextServices,
                base::Unretained(this)));
  }
  ~SettingsOverriddenExplicitChoiceDialogInteractiveUiTest() override = default;

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    SetUpMockImageFetcher();
    search_test_utils::WaitForTemplateURLServiceToLoad(
        TemplateURLServiceFactory::GetForProfile(browser()->profile()));
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);
    ImageFetcherServiceFactory::GetInstance()->SetTestingFactory(
        profile->GetProfileKey(),
        base::BindRepeating(
            [](SimpleFactoryKey* key) -> std::unique_ptr<KeyedService> {
              return std::make_unique<MockImageFetcherService>();
            }));
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
      ui_test_utils::SendToOmniboxAndSubmit(
          browser(), "Penguin", base::TimeTicks::Now(),
          /*wait_for_autocomplete_done=*/false);
    });
  }

  // Waits for the dialog to show, and verifies it's explicit-choice type.
  auto WaitForDialogToShow() {
    auto steps =
        Steps(WaitForShow(kSettingsOverriddenDialogId),
              EnsurePresent(kSettingsOverriddenDialogSaveButtonId),
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
                   /*baseline_cl=*/"7563250"));
    return steps;
  }

 private:
  // Sets up the mock ImageFetcher so that we can exercise icon fetching, rather
  // than having the dialog fall back to generated placeholder icons. Note that
  // we could also try and do this end-to-end by setting up URL fetch
  // intercepters, but that's more complexity and fragility just to include
  // ImageFetcher in this dialog testing.
  void SetUpMockImageFetcher() {
    auto* service = static_cast<MockImageFetcherService*>(
        ImageFetcherServiceFactory::GetForKey(
            browser()->profile()->GetProfileKey()));
    // These icons are arbitrary. The goal is to ensure icons are fetched, vs.
    // falling back to generated placeholder icons.
    constexpr int kFaviconSize = 32;
    gfx::Image icon(gfx::CreateVectorIcon(vector_icons::kBusinessIcon,
                                          kFaviconSize, SK_ColorRED));
    gfx::Image google_icon(gfx::CreateVectorIcon(vector_icons::kGoogleColorIcon,
                                                 kFaviconSize, SK_ColorBLUE));

    EXPECT_CALL(*service->mock_image_fetcher(), FetchImageAndData_(_, _, _, _))
        .WillRepeatedly([icon, google_icon](
                            const GURL& url,
                            image_fetcher::ImageDataFetcherCallback*,
                            image_fetcher::ImageFetcherCallback* callback,
                            image_fetcher::ImageFetcherParams) {
          const gfx::Image& image =
              url.host().find("google.com") != std::string::npos ? google_icon
                                                                 : icon;
          std::move(*callback).Run(image, image_fetcher::RequestMetadata());
        });
  }

 private:
  base::CallbackListSubscription create_services_subscription_;
  base::test::ScopedFeatureList feature_list_;
  extensions::ScopedTestMV2Enabler mv2_enabler_;
};

IN_PROC_BROWSER_TEST_F(SettingsOverriddenExplicitChoiceDialogInteractiveUiTest,
                       SearchOverriddenDialogWhenPreviouslyGoogle) {
  RunTestSequence(SetNewSearchProvider(DefaultSearch::kUseDefault),
                  LoadExtensionOverridingSearch(), PerformSearchFromOmnibox(),
                  WaitForDialogToShow(), ScreenshotDialog());
}

IN_PROC_BROWSER_TEST_F(SettingsOverriddenExplicitChoiceDialogInteractiveUiTest,
                       SearchOverriddenDialogWhenPreviouslyNonGoogle) {
  RunTestSequence(
      SetNewSearchProvider(DefaultSearch::kUseNonGoogleFromDefaultList),
      LoadExtensionOverridingSearch(), PerformSearchFromOmnibox(),
      WaitForDialogToShow(), ScreenshotDialog());
}

IN_PROC_BROWSER_TEST_F(SettingsOverriddenExplicitChoiceDialogInteractiveUiTest,
                       SearchOverriddenDialogWhenPreviouslyOldExtension) {
  RunTestSequence(SetNewSearchProvider(DefaultSearch::kUseNewSearch),
                  LoadExtensionOverridingSearch(), PerformSearchFromOmnibox(),
                  WaitForDialogToShow(), ScreenshotDialog());
}

IN_PROC_BROWSER_TEST_F(SettingsOverriddenExplicitChoiceDialogInteractiveUiTest,
                       SelectOption) {
  // This test covers the unconventional explicit-choice dialog behavior,
  // having no initially-selected radio button.
  RunTestSequence(
      SetNewSearchProvider(DefaultSearch::kUseDefault),
      LoadExtensionOverridingSearch(), PerformSearchFromOmnibox(),
      WaitForDialogToShow(),
      // Assert that neither radio button is initially selected or focused.
      CheckViewProperty(kSettingsOverriddenDialogPreviousSettingButtonId,
                        &views::View::HasFocus, false),
      CheckViewProperty(kSettingsOverriddenDialogPreviousSettingButtonId,
                        &extensions::RichRadioButton::GetCheckedForTesting,
                        false),
      CheckViewProperty(kSettingsOverriddenDialogNewSettingButtonId,
                        &views::View::HasFocus, false),
      CheckViewProperty(kSettingsOverriddenDialogNewSettingButtonId,
                        &extensions::RichRadioButton::GetCheckedForTesting,
                        false),
      // Assert that the Save button is disabled initially. Choosing an
      // option enables it.
      CheckViewProperty(kSettingsOverriddenDialogSaveButtonId,
                        &views::View::GetEnabled, false),
      // Select an option.
      PressButton(kSettingsOverriddenDialogNewSettingButtonId),
      // Ensure the selected option is checked (but not focused).
      CheckViewProperty(kSettingsOverriddenDialogNewSettingButtonId,
                        &views::View::HasFocus, false),
      CheckViewProperty(kSettingsOverriddenDialogNewSettingButtonId,
                        &extensions::RichRadioButton::GetCheckedForTesting,
                        true),
      // Assert that the save button is now enabled.
      CheckViewProperty(kSettingsOverriddenDialogSaveButtonId,
                        &views::View::GetEnabled, true),
      // Click the save button.
      PressButton(kSettingsOverriddenDialogSaveButtonId),
      WaitForHide(kSettingsOverriddenDialogSaveButtonId));
}

}  // namespace
