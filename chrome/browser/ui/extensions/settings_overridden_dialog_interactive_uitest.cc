// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/i18n/rtl.h"
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
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/extensions/rich_radio_button.h"
#include "chrome/common/chrome_features.h"
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
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace {

// The extension in "extensions/search_provider_override" uses "example.com"
// as the search URL.
constexpr char kExtensionSearchUrl[] = "https://www.example.com/?q=Penguin";

enum class DefaultSearch {
  kUseDefault,
  kUseNonGoogleFromDefaultList,
  kUseNewSearch,
};

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsId);

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
 protected:
  const ui::ElementIdentifier kPreviousSettingButtonId =
      kSettingsOverriddenDialogPreviousSettingButtonId;
  const ui::ElementIdentifier kNewSettingButtonId =
      kSettingsOverriddenDialogNewSettingButtonId;
  const ui::ElementIdentifier kSaveButtonId =
      kSettingsOverriddenDialogSaveButtonId;

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

  virtual void OnWillCreateBrowserContextServices(
      content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);
    ImageFetcherServiceFactory::GetInstance()->SetTestingFactory(
        profile->GetProfileKey(),
        base::BindRepeating(
            [](SimpleFactoryKey* key) -> std::unique_ptr<KeyedService> {
              return std::make_unique<MockImageFetcherService>();
            }));
  }

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
    auto steps = Steps(WaitForShow(kSettingsOverriddenDialogId),
                       EnsurePresent(kSaveButtonId),
                       EnsurePresent(kPreviousSettingButtonId),
                       EnsurePresent(kNewSettingButtonId));
    AddDescriptionPrefix(steps, __func__);
    return steps;
  }

  auto ScreenshotDialog() {
    auto steps =
        Steps(SetOnIncompatibleAction(
                  OnIncompatibleAction::kIgnoreAndContinue,
                  "Screenshot can only run in pixel_tests on Windows."),
              ScreenshotSurface(
                  kSettingsOverriddenDialogId,
                  /*screenshot_name=*/"ExtensionSettingsOverridenDialog",
                  /*baseline_cl=*/"7568622"));
    return steps;
  }

  auto CheckActiveUrl(const GURL& expected_url) {
    return CheckResult(
        [this]() {
          return browser()
              ->tab_strip_model()
              ->GetActiveWebContents()
              ->GetLastCommittedURL();
        },
        expected_url,
        "Check that navigation is deferred and hasn't reached the extension "
        "URL");
  }

  // Some Google search URL parameters are dynamic. Therefore, for testing
  // purpose, the host will be used for validation.
  auto CheckWebContentsNavigateToGoogle() {
    return CheckResult(
        [this]() {
          return browser()
              ->tab_strip_model()
              ->GetActiveWebContents()
              ->GetLastCommittedURL()
              .host();
        },
        "www.google.com", "Wait for navigation to Google");
  }

  auto CheckFocused(ui::ElementIdentifier id, bool is_focused) {
    return CheckView(id, [is_focused](views::View* view) {
      const views::View* focused_view =
          view->GetFocusManager()->GetFocusedView();
      bool contains_focus = focused_view && (view == focused_view ||
                                             view->Contains(focused_view));
      return contains_focus == is_focused;
    });
  }

  auto CheckSelected(ui::ElementIdentifier id, bool is_selected) {
    return CheckViewProperty(
        id, &extensions::RichRadioButton::GetCheckedForTesting, is_selected);
  }

  auto CheckSelectedAndFocused(ui::ElementIdentifier id,
                               bool is_selected_and_focused) {
    return Steps(CheckSelected(id, is_selected_and_focused),
                 CheckFocused(id, is_selected_and_focused));
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
                       WhenPreviouslyGoogleChoosingPreviousRestoresGoogle) {
  RunTestSequence(InstrumentTab(kWebContentsId),
                  SetNewSearchProvider(DefaultSearch::kUseDefault),
                  LoadExtensionOverridingSearch(), PerformSearchFromOmnibox(),
                  WaitForDialogToShow(), CheckActiveUrl(GURL("about:blank")),
                  ScreenshotDialog(), PressButton(kPreviousSettingButtonId),
                  // Click Save.
                  PressButton(kSaveButtonId),
                  WaitForHide(kSettingsOverriddenDialogId),
                  // Verify navigation proceeds to the default search URL
                  // (Google) instead of the extension URL.
                  WaitForWebContentsNavigation(kWebContentsId),
                  CheckWebContentsNavigateToGoogle());
}

IN_PROC_BROWSER_TEST_F(
    SettingsOverriddenExplicitChoiceDialogInteractiveUiTest,
    WhenPreviouslyNonGoogleChoosingPreviousRestoresNonGoogle) {
  RunTestSequence(
      InstrumentTab(kWebContentsId),
      SetNewSearchProvider(DefaultSearch::kUseNonGoogleFromDefaultList),
      LoadExtensionOverridingSearch(), PerformSearchFromOmnibox(),
      WaitForDialogToShow(), CheckActiveUrl(GURL("about:blank")),
      ScreenshotDialog(),
      // Select previous search setting.
      PressButton(kNewSettingButtonId), PressButton(kSaveButtonId),
      WaitForHide(kSettingsOverriddenDialogId),
      // Verify navigation proceeds to the extension's search URL.
      WaitForWebContentsNavigation(kWebContentsId, GURL(kExtensionSearchUrl)));
}

IN_PROC_BROWSER_TEST_F(SettingsOverriddenExplicitChoiceDialogInteractiveUiTest,
                       WhenPreviouslyOldExtension) {
  RunTestSequence(SetNewSearchProvider(DefaultSearch::kUseNewSearch),
                  LoadExtensionOverridingSearch(), PerformSearchFromOmnibox(),
                  WaitForDialogToShow(), CheckActiveUrl(GURL("about:blank")),
                  ScreenshotDialog());
}

IN_PROC_BROWSER_TEST_F(SettingsOverriddenExplicitChoiceDialogInteractiveUiTest,
                       SelectOption) {
  // This test covers the unconventional explicit-choice dialog behavior,
  // having no initially-selected radio button.
  RunTestSequence(
      InstrumentTab(kWebContentsId),
      SetNewSearchProvider(DefaultSearch::kUseDefault),
      LoadExtensionOverridingSearch(), PerformSearchFromOmnibox(),
      WaitForDialogToShow(), CheckActiveUrl(GURL("about:blank")),
      // Assert that neither radio button is initially selected or focused.
      CheckSelectedAndFocused(kPreviousSettingButtonId, false),
      CheckSelectedAndFocused(kNewSettingButtonId, false),
      // Assert that the Save button is disabled initially. Choosing an
      // option enables it.
      CheckViewProperty(kSaveButtonId, &views::View::GetEnabled, false),
      // Select an option.
      PressButton(kNewSettingButtonId),
      // Ensure the selected option is checked (but not focused).
      CheckSelected(kNewSettingButtonId, true),
      CheckFocused(kNewSettingButtonId, false),
      // Assert that the save button is now enabled.
      CheckViewProperty(kSaveButtonId, &views::View::GetEnabled, true),
      // Click the save button.
      PressButton(kSaveButtonId), WaitForHide(kSettingsOverriddenDialogId),
      WaitForWebContentsNavigation(kWebContentsId, GURL(kExtensionSearchUrl)));
}

IN_PROC_BROWSER_TEST_F(SettingsOverriddenExplicitChoiceDialogInteractiveUiTest,
                       EscapeDoesNotCloseDialog) {
  // This test verifies that pressing Escape does not close the dialog.
  RunTestSequence(
      InstrumentTab(kWebContentsId),
      SetNewSearchProvider(DefaultSearch::kUseDefault),
      LoadExtensionOverridingSearch(), PerformSearchFromOmnibox(),
      WaitForDialogToShow(),
      // Send Escape.
      SendKeyPress(kSettingsOverriddenDialogId, ui::VKEY_ESCAPE),
      // Verify dialog is still present (Wait a bit or just ensure present).
      EnsurePresent(kSettingsOverriddenDialogId),
      // Clean up by choosing an option so the dialog closes and the navigation
      // finishes.
      PressButton(kNewSettingButtonId), PressButton(kSaveButtonId),
      WaitForHide(kSettingsOverriddenDialogId),
      WaitForWebContentsNavigation(kWebContentsId, GURL(kExtensionSearchUrl)));
}

IN_PROC_BROWSER_TEST_F(SettingsOverriddenExplicitChoiceDialogInteractiveUiTest,
                       DialogShownOnNonNewTabPage) {
  // Test that if we are on a real website (e.g. google.com) and perform a
  // search that is intercepted by the extension, the visible URL remains
  // on the existing page until the dialog is resolved.
  const GURL kInitialUrl("https://www.google.com/");

  RunTestSequence(
      InstrumentTab(kWebContentsId),
      NavigateWebContents(kWebContentsId, kInitialUrl),
      SetNewSearchProvider(DefaultSearch::kUseDefault),
      LoadExtensionOverridingSearch(), PerformSearchFromOmnibox(),
      WaitForDialogToShow(),
      // Visible URL should still be the initial site, not the extension's
      // search.
      CheckActiveUrl(kInitialUrl),
      // Select previous search setting.
      PressButton(kNewSettingButtonId), PressButton(kSaveButtonId),
      WaitForHide(kSettingsOverriddenDialogId),
      // Only now should the navigation complete.
      WaitForWebContentsNavigation(kWebContentsId, GURL(kExtensionSearchUrl)));
}

class SettingsOverriddenExplicitChoiceDialogHatsInteractiveUiTest
    : public SettingsOverriddenExplicitChoiceDialogInteractiveUiTest {
 public:
  SettingsOverriddenExplicitChoiceDialogHatsInteractiveUiTest() {
    feature_list_.InitAndEnableFeature(
        features::kHappinessTrackingSurveysForDesktopSEHijacking);
  }

  void OnWillCreateBrowserContextServices(
      content::BrowserContext* context) override {
    SettingsOverriddenExplicitChoiceDialogInteractiveUiTest::
        OnWillCreateBrowserContextServices(context);
    Profile* profile = Profile::FromBrowserContext(context);
    HatsServiceFactory::GetInstance()->SetTestingFactory(
        profile, base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
          return std::make_unique<testing::NiceMock<MockHatsService>>(
              Profile::FromBrowserContext(context));
        }));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    SettingsOverriddenExplicitChoiceDialogHatsInteractiveUiTest,
    HatsSurveyTriggered) {
  RunTestSequence(
      SetNewSearchProvider(DefaultSearch::kUseDefault),
      LoadExtensionOverridingSearch(), PerformSearchFromOmnibox(),
      WaitForDialogToShow(), Do([this]() {
        HatsService* hats_service = HatsServiceFactory::GetForProfile(
            browser()->profile(), /*create_if_necessary=*/true);
        CHECK(hats_service);
        MockHatsService* mock_hats_service =
            static_cast<MockHatsService*>(hats_service);
        EXPECT_CALL(
            *mock_hats_service,
            LaunchDelayedSurvey(kHatsSurveyTriggerSEHijacking, 5000, _, _))
            .WillOnce(testing::Return(true));
      }),
      PressButton(kNewSettingButtonId), PressButton(kSaveButtonId),
      WaitForHide(kSaveButtonId));
}

// Ensures that the dialog operates correctly under keyboard input.
IN_PROC_BROWSER_TEST_F(SettingsOverriddenExplicitChoiceDialogInteractiveUiTest,
                       VerifyKeyboardInteraction) {
  auto kFirstOption = kPreviousSettingButtonId;
  auto kSecondOption = kNewSettingButtonId;

  RunTestSequence(
      InstrumentTab(kWebContentsId),
      SetNewSearchProvider(DefaultSearch::kUseDefault),
      LoadExtensionOverridingSearch(), PerformSearchFromOmnibox(),
      WaitForDialogToShow(), CheckActiveUrl(GURL("about:blank")),

      // Initially, nothing is focused or selected.
      CheckSelectedAndFocused(kFirstOption, false),
      CheckSelectedAndFocused(kSecondOption, false),

      // Press Tab. First radio button should focus (and become selected).
      SendKeyPress(kSettingsOverriddenDialogId, ui::VKEY_TAB),
      CheckSelectedAndFocused(kFirstOption, true),
      CheckSelectedAndFocused(kSecondOption, false),

      // Press Tab. Save button should be focused.
      SendKeyPress(kSettingsOverriddenDialogId, ui::VKEY_TAB),
      CheckSelected(kFirstOption, true), CheckFocused(kFirstOption, false),
      CheckSelectedAndFocused(kSecondOption, false),

      // Press Tab. First radio button should focus.
      SendKeyPress(kSettingsOverriddenDialogId, ui::VKEY_TAB),
      CheckSelectedAndFocused(kFirstOption, true),
      CheckSelectedAndFocused(kSecondOption, false),

      // Press Down Arrow key. Second radio button should now be focused and
      // selected.
      SendKeyPress(kSettingsOverriddenDialogId, ui::VKEY_DOWN),
      CheckSelectedAndFocused(kFirstOption, false),
      CheckSelectedAndFocused(kSecondOption, true),
      // Press Right Arrow key. First radio button should now be focused and
      // selected, as focus cycles back to the first element.
      SendKeyPress(kSettingsOverriddenDialogId, ui::VKEY_RIGHT),
      CheckSelectedAndFocused(kFirstOption, true),
      CheckSelectedAndFocused(kSecondOption, false),

      // Finish the dialog to clean up.
      PressButton(kSaveButtonId), WaitForHide(kSettingsOverriddenDialogId));
}

IN_PROC_BROWSER_TEST_F(SettingsOverriddenExplicitChoiceDialogInteractiveUiTest,
                       RtlLayout) {
  base::i18n::SetRTLForTesting(true);
  RunTestSequence(InstrumentTab(kWebContentsId),
                  SetNewSearchProvider(DefaultSearch::kUseDefault),
                  LoadExtensionOverridingSearch(), PerformSearchFromOmnibox(),
                  WaitForDialogToShow(), ScreenshotDialog());
}

}  // namespace
