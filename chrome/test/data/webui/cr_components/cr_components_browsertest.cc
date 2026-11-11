// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/strings/strcat.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/searchbox_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "chrome/test/base/web_ui_test_data_source.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/scoped_web_ui_controller_factory_registration.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "ui/webui/webui_util.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/webui/common/trusted_types_util.h"
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "base/command_line.h"
#include "chrome/browser/browser_features.h"
#include "components/history_clusters/core/features.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/webui/buildflags.h"
#if BUILDFLAG(ENABLE_WEBUI_NTP)
#include "chrome/browser/ui/webui/cr_components/most_visited/most_visited_handler.h"
#endif
#endif

namespace {

// Base WebUIController for chrome://webui-test that enables Mojo JS bindings
// and chrome.histograms without depending on a feature-specific WebUI host.
class TestMojoWebUIController : public ui::MojoWebUIController {
 public:
  explicit TestMojoWebUIController(content::WebUI* web_ui)
      : ui::MojoWebUIController(web_ui,
                                /*enable_chrome_send=*/false,
                                /*enable_chrome_histograms=*/true) {}

 protected:
  content::WebUIDataSource* CreateWebUITestDataSource() {
    content::WebUIDataSource* source =
        webui::CreateAndAddWebUITestDataSource(Profile::FromWebUI(web_ui()));
    source->UseStringsJs();
    return source;
  }
};

class TestComposeboxWebUIController : public TestMojoWebUIController {
 public:
  explicit TestComposeboxWebUIController(content::WebUI* web_ui)
      : TestMojoWebUIController(web_ui) {
    content::WebUIDataSource* source = CreateWebUITestDataSource();
#if BUILDFLAG(IS_CHROMEOS)
    source->OverrideContentSecurityPolicy(
        network::mojom::CSPDirectiveName::TrustedTypes,
        base::StrCat({ash::kDefaultTrustedTypesPolicies,
                      " lit-html-desktop composebox-input;"}));
#else
    source->OverrideContentSecurityPolicy(
        network::mojom::CSPDirectiveName::TrustedTypes,
        base::StrCat(
            {webui::kDefaultTrustedTypesPolicies, " composebox-input;"}));
#endif
    source->AddLocalizedStrings(SearchboxHandler::GetWebUIDataSourceDict(
        Profile::FromWebUI(web_ui), {.enable_voice_search = true}));

    // Composebox embedder-configured flags and params not set in
    // SearchboxHandler::GetWebUIDataSourceDict().
    source->AddString("composeboxImageFileTypes",
                      "image/jpeg,image/png,image/heic,image/heif,.heic,.heif");
    source->AddString("composeboxAttachmentFileTypes",
                      "text/plain,application/pdf");
    source->AddInteger("composeboxFileMaxSize", 100 * 1024 * 1024);
    source->AddBoolean("composeboxShowTypedSuggest", false);
    source->AddBoolean("composeboxShowZps", true);
    source->AddBoolean("composeboxShowImageSuggest", true);
    source->AddBoolean("composeboxShowContextMenu", true);
    source->AddBoolean("composeboxShowContextMenuDescription", false);
    source->AddBoolean("composeboxShowContextMenuTabPreviews", false);
    source->AddBoolean("composeboxContextMenuEnableMultiTabSelection", true);
    source->AddBoolean("composeboxContextMenuEnableTabDeselection", false);
    source->AddBoolean("composeboxContextMenuTooltipsEnabled", false);
    source->AddBoolean("contextMenuAnimationLimitingEnabled", true);
    source->AddBoolean("composeboxSkillsEnabled", false);
    source->AddBoolean("composeboxSmartComposeEnabled", true);
    source->AddBoolean("composeboxSmartTabSharingSupported",
                       !BUILDFLAG(IS_ANDROID));
    source->AddBoolean("supportsLensButtonInComposebox",
                       !BUILDFLAG(IS_ANDROID));
    source->AddBoolean("isSystemVoiceSearchEnabled", false);
    source->AddBoolean("androidSpeechRecognition", false);
    source->AddBoolean("clearAllInputsWhenSubmittingQuery", false);
    source->AddBoolean("lensSendRawFileMediaTypesEnabled", false);
    source->AddBoolean("isLensSearchbox", true);
    source->AddBoolean("contextManagementInComposeboxEnabled", false);
    source->AddBoolean("tabFaviconChipsToCoinsEnabled", true);
    source->AddBoolean("webUIOmniboxAskGAboutThisPageEnabled", false);
    source->AddBoolean("contextualTasksUpdatedEntryPointsEnabled", false);
    source->AddString("composeboxSource", "ContextualTasks");
    source->AddString("aimThreadsGoogleSearchLabel", "Google Search");
    source->AddString("composeboxHintTextLensOverlay",
                      "Select anything on the page");
  }

  WEB_UI_CONTROLLER_TYPE_DECL();
};

WEB_UI_CONTROLLER_TYPE_IMPL(TestComposeboxWebUIController)

content::ScopedWebUIConfigRegistration RegisterComposeboxTestConfig() {
  return content::ScopedWebUIConfigRegistration(
      std::make_unique<
          content::DefaultWebUIConfig<TestComposeboxWebUIController>>(
          content::kChromeUIScheme, chrome::kChromeUIWebUITestHost));
}

#if !BUILDFLAG(IS_ANDROID)
content::ScopedWebUIConfigRegistration RegisterMojoTestConfig() {
  return content::ScopedWebUIConfigRegistration(
      std::make_unique<content::DefaultWebUIConfig<TestMojoWebUIController>>(
          content::kChromeUIScheme, chrome::kChromeUIWebUITestHost));
}

class TestSearchboxWebUIController : public TestMojoWebUIController {
 public:
  explicit TestSearchboxWebUIController(content::WebUI* web_ui)
      : TestMojoWebUIController(web_ui) {
    content::WebUIDataSource* source = CreateWebUITestDataSource();
    source->AddLocalizedStrings(SearchboxHandler::GetWebUIDataSourceDict(
        Profile::FromWebUI(web_ui), {.enable_voice_search = true}));

    // Searchbox embedder-configured flags not set in
    // SearchboxHandler::GetWebUIDataSourceDict().
    source->AddBoolean("isFuseboxEnabled", false);
    source->AddBoolean("energyEffectAnimationEnabled", false);
  }

  WEB_UI_CONTROLLER_TYPE_DECL();
};

WEB_UI_CONTROLLER_TYPE_IMPL(TestSearchboxWebUIController)

content::ScopedWebUIConfigRegistration RegisterSearchboxTestConfig() {
  return content::ScopedWebUIConfigRegistration(
      std::make_unique<
          content::DefaultWebUIConfig<TestSearchboxWebUIController>>(
          content::kChromeUIScheme, chrome::kChromeUIWebUITestHost));
}

#if BUILDFLAG(ENABLE_WEBUI_NTP)
class TestMostVisitedWebUIController : public TestMojoWebUIController {
 public:
  explicit TestMostVisitedWebUIController(content::WebUI* web_ui)
      : TestMojoWebUIController(web_ui) {
    content::WebUIDataSource* source = CreateWebUITestDataSource();
    source->AddLocalizedStrings(MostVisitedHandler::GetWebUIDataSourceDict());
  }

  WEB_UI_CONTROLLER_TYPE_DECL();
};

WEB_UI_CONTROLLER_TYPE_IMPL(TestMostVisitedWebUIController)
#endif  // BUILDFLAG(ENABLE_WEBUI_NTP)
#endif

}  // namespace

typedef WebUIMochaBrowserTest CrComponentsTest;

IN_PROC_BROWSER_TEST_F(CrComponentsTest, ColorChangeListener) {
  set_test_loader_host(chrome::kChromeUIExtensionsHost);
  RunTest("cr_components/color_change_listener_test.js", "mocha.run()");
}

#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(CrComponentsTest, CustomizeColorSchemeMode) {
  set_test_loader_host(chrome::kChromeUICustomizeChromeSidePanelHost);
  RunTest("cr_components/customize_color_scheme_mode_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsTest, HelpBubbleMixin) {
  auto config = RegisterMojoTestConfig();
  RunTest("cr_components/help_bubble/help_bubble_mixin_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsTest, HelpBubble) {
  RunTest("cr_components/help_bubble/help_bubble_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsTest, HorizontalCarousel) {
  RunTest("cr_components/history_clusters/horizontal_carousel_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsTest, AudioLevelsProcessorTest) {
  RunTest("cr_components/composebox/composebox_audio_levels_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsTest, ManagedDialog) {
  RunTest("cr_components/managed_dialog_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsTest, ManagedFootnote) {
  // Loaded from chrome://settings because it needs access to chrome.send().
  set_test_loader_host(chrome::kChromeUISettingsHost);
  RunTest("cr_components/managed_footnote_test.js", "mocha.run()");
}
#endif  // !BUILDFLAG(IS_ANDROID)

IN_PROC_BROWSER_TEST_F(CrComponentsTest, LocalizedLink) {
  RunTest("cr_components/localized_link_test.js", "mocha.run()");
}

#if !BUILDFLAG(IS_ANDROID)
using CrComponentsSearchboxTest = WebUIMochaBrowserTest;

IN_PROC_BROWSER_TEST_F(CrComponentsSearchboxTest, RealboxMatchTest) {
  auto config = RegisterSearchboxTestConfig();
  RunTest("cr_components/searchbox/searchbox_match_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsSearchboxTest, SearchboxInputTest) {
  auto config = RegisterSearchboxTestConfig();
  RunTest("cr_components/searchbox/searchbox_input_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsSearchboxTest, RealboxPlaceholderTest) {
  auto config = RegisterSearchboxTestConfig();
  RunTest("cr_components/searchbox/placeholder_text_cycler_test.js",
          "runMochaSuite('PlaceholderTextCyclerTest');");
}

IN_PROC_BROWSER_TEST_F(CrComponentsSearchboxTest, SearchboxDropdownTest) {
  auto config = RegisterSearchboxTestConfig();
  RunTest("cr_components/searchbox/searchbox_dropdown_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsSearchboxTest, SearchboxSelectionMixin) {
  RunTest("cr_components/searchbox/searchbox_selection_mixin_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsSearchboxTest, KeywordModeManagerTest) {
  auto config = RegisterSearchboxTestConfig();
  RunTest("cr_components/searchbox/keyword_mode_manager_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsSearchboxTest, SearchboxIconTest) {
  auto config = RegisterSearchboxTestConfig();
  RunTest("cr_components/searchbox/searchbox_icon_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsSearchboxTest, SearchboxMixinTest) {
  auto config = RegisterSearchboxTestConfig();
  RunTest("cr_components/searchbox/searchbox_mixin_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsSearchboxTest, SearchboxUtilsTest) {
  auto config = RegisterSearchboxTestConfig();
  RunTest("cr_components/searchbox/searchbox_utils_test.js", "mocha.run()");
}

class CrComponentsHistoryClustersTest : public WebUIMochaBrowserTest {
 protected:
  CrComponentsHistoryClustersTest() {
    scoped_feature_list_.InitAndEnableFeature(
        history_clusters::internal::kJourneysImages);
    set_test_loader_host(chrome::kChromeUIHistoryHost);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(crbug.com/390550686): Test is flaky.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_All DISABLED_All
#else
#define MAYBE_All All
#endif
IN_PROC_BROWSER_TEST_F(CrComponentsHistoryClustersTest, MAYBE_All) {
  RunTest("cr_components/history_clusters/history_clusters_test.js",
          "runMochaSuite('HistoryClustersTest')");
}

IN_PROC_BROWSER_TEST_F(CrComponentsHistoryClustersTest, Cluster) {
  RunTest("cr_components/history_clusters/cluster_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsHistoryClustersTest, UrlVisit) {
  RunTest("cr_components/history_clusters/url_visit_test.js", "mocha.run()");
}

#if BUILDFLAG(ENABLE_WEBUI_NTP)
class CrComponentsMostVisitedTest : public WebUIMochaBrowserTest {
 private:
  content::ScopedWebUIConfigRegistration config_{std::make_unique<
      content::DefaultWebUIConfig<TestMostVisitedWebUIController>>(
      content::kChromeUIScheme,
      chrome::kChromeUIWebUITestHost)};
};

IN_PROC_BROWSER_TEST_F(CrComponentsMostVisitedTest, General) {
  RunTest("cr_components/most_visited_test.js", "runMochaSuite('General');");
}

// TODO(https://crbug.com/449760234): Times out on linux debug builds.
#if BUILDFLAG(IS_LINUX) && !defined(NDEBUG)
#define MAYBE_Layouts DISABLED_Layouts
#else
#define MAYBE_Layouts Layouts
#endif
IN_PROC_BROWSER_TEST_F(CrComponentsMostVisitedTest, MAYBE_Layouts) {
  RunTest("cr_components/most_visited_test.js", "runMochaSuite('Layouts');");
}

IN_PROC_BROWSER_TEST_F(CrComponentsMostVisitedTest, ReflowLayouts) {
  RunTest("cr_components/most_visited_test.js",
          "runMochaSuite('Reflow Layouts');");
}

IN_PROC_BROWSER_TEST_F(CrComponentsMostVisitedTest, ContainerWidth) {
  RunTest("cr_components/most_visited_test.js",
          "runMochaSuite('ContainerWidth');");
}

IN_PROC_BROWSER_TEST_F(CrComponentsMostVisitedTest, LoggingAndUpdates) {
  RunTest("cr_components/most_visited_test.js",
          "runMochaSuite('LoggingAndUpdates');");
}

// crbug.com/40776780
#if BUILDFLAG(IS_LINUX) && !defined(NDEBUG)
#define MAYBE_Modification DISABLED_Modification
#else
#define MAYBE_Modification Modification
#endif
IN_PROC_BROWSER_TEST_F(CrComponentsMostVisitedTest, MAYBE_Modification) {
  RunTest("cr_components/most_visited_test.js",
          "runMochaSuite('Modification');");
}

IN_PROC_BROWSER_TEST_F(CrComponentsMostVisitedTest, DragAndDrop) {
  RunTest("cr_components/most_visited_test.js",
          "runMochaSuite('DragAndDrop');");
}

IN_PROC_BROWSER_TEST_F(CrComponentsMostVisitedTest, EnterpriseShortcuts) {
  RunTest("cr_components/most_visited_test.js",
          "runMochaSuite('EnterpriseShortcuts');");
}

IN_PROC_BROWSER_TEST_F(CrComponentsMostVisitedTest, Theming) {
  RunTest("cr_components/most_visited_test.js", "runMochaSuite('Theming');");
}

IN_PROC_BROWSER_TEST_F(CrComponentsMostVisitedTest, ShowAddButton) {
  RunTest("cr_components/most_visited_test.js",
          "runMochaSuite('ShowAddButton');");
}

IN_PROC_BROWSER_TEST_F(CrComponentsMostVisitedTest, ExpandableTiles) {
  RunTest("cr_components/most_visited_test.js",
          "runMochaSuite('ExpandableTiles');");
}

IN_PROC_BROWSER_TEST_F(CrComponentsMostVisitedTest, ShortcutsAutoRemovalToast) {
  RunTest("cr_components/most_visited_test.js",
          "runMochaSuite('ShortcutsAutoRemovalToast');");
}

IN_PROC_BROWSER_TEST_F(CrComponentsMostVisitedTest, ShortcutsReorder) {
  RunTest("cr_components/most_visited_test.js",
          "runMochaSuite('ShortcutsReorder');");
}

IN_PROC_BROWSER_TEST_F(CrComponentsMostVisitedTest, NonEditable) {
  RunTest("cr_components/most_visited_test.js",
          "runMochaSuite('NonEditable');");
}

#endif  // BUILDFLAG(ENABLE_WEBUI_NTP)

typedef WebUIMochaBrowserTest CrComponentsThemeColorPickerTest;
IN_PROC_BROWSER_TEST_F(CrComponentsThemeColorPickerTest, ThemeColor) {
  set_test_loader_host(chrome::kChromeUICustomizeChromeSidePanelHost);
  RunTest("cr_components/theme_color_picker/theme_color_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsThemeColorPickerTest, CheckMarkWrapper) {
  set_test_loader_host(chrome::kChromeUICustomizeChromeSidePanelHost);
  RunTest("cr_components/theme_color_picker/check_mark_wrapper_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsThemeColorPickerTest, ThemeColorPicker) {
  set_test_loader_host(chrome::kChromeUICustomizeChromeSidePanelHost);
  RunTest("cr_components/theme_color_picker/theme_color_picker_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsThemeColorPickerTest, ThemeHueSliderDialog) {
  set_test_loader_host(chrome::kChromeUICustomizeChromeSidePanelHost);
  RunTest("cr_components/theme_color_picker/theme_hue_slider_dialog_test.js",
          "mocha.run()");
}

#if BUILDFLAG(ENABLE_WEBUI_NTP)
class CrComponentsPreloadingTest : public CrComponentsMostVisitedTest {
 protected:
  CrComponentsPreloadingTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {base::test::FeatureRefAndParams(
             features::kNewTabPageTriggerForPrerender2,
             {{"preconnect_start_delay_on_mouse_hover_ms", "0"}}),
         base::test::FeatureRefAndParams(
             features::kNewTabPageTriggerForPrefetch,
             {{"prefetch_start_delay_on_mouse_hover_ms", "0"}})},
        {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(CrComponentsPreloadingTest, Preloading) {
  RunTest("cr_components/most_visited_test.js", "runMochaSuite('Preloading');");
}
#endif  // BUILDFLAG(ENABLE_WEBUI_NTP)
#endif  // !BUILDFLAG(IS_ANDROID)

using CrComponentsComposeboxTest = WebUIMochaBrowserTest;

IN_PROC_BROWSER_TEST_F(CrComponentsComposeboxTest, ContextualEntrypointButton) {
  auto config = RegisterComposeboxTestConfig();
  RunTest("cr_components/composebox/contextual_entrypoint_button_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsComposeboxTest,
                       ContextualEntrypointAndMenu) {
  auto config = RegisterComposeboxTestConfig();
  RunTest("cr_components/composebox/contextual_entrypoint_and_menu_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsComposeboxTest, ComposeboxFaviconGroup) {
  RunTest("cr_components/composebox/composebox_favicon_group_test.js",
          "mocha.run()");
}

// TODO(crbug.com/513266451): Enable for Android.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(CrComponentsComposeboxTest, ContextualActionMenu) {
  auto config = RegisterComposeboxTestConfig();
  RunTest("cr_components/composebox/contextual_action_menu_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsComposeboxTest,
                       ComposeboxAudioWaveAnimation) {
  RunTest("cr_components/composebox/composebox_audio_animation_test.js",
          "mocha.run()");
}

typedef WebUIMochaBrowserTest CrComponentsSearchTest;
IN_PROC_BROWSER_TEST_F(CrComponentsSearchTest, AnimatedGlow) {
  RunTest("cr_components/search/animated_glow_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsSearchTest, RecordingWave) {
  RunTest("cr_components/search/recording_wave_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsComposeboxTest, LensSearch) {
  RunTest("cr_components/composebox/composebox_lens_search_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsComposeboxTest, CurrentTabChip) {
  auto config = RegisterComposeboxTestConfig();
  RunTest("cr_components/composebox/current_tab_chip_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsComposeboxTest, ComposeboxToolChip) {
  auto config = RegisterComposeboxTestConfig();
  RunTest("cr_components/composebox/composebox_tool_chip_test.js",
          "mocha.run()");
}
#endif

IN_PROC_BROWSER_TEST_F(CrComponentsComposeboxTest, ComposeboxVoiceSearch) {
  auto config = RegisterComposeboxTestConfig();
  RunTest("cr_components/composebox/composebox_voice_search_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsComposeboxTest,
                       ComposeboxVoiceSearchMetrics) {
  auto config = RegisterComposeboxTestConfig();
  RunTest("cr_components/composebox/composebox_voice_search_metrics_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsComposeboxTest,
                       ComposeboxVoiceSearchRecognition) {
  auto config = RegisterComposeboxTestConfig();
  RunTest(
      "cr_components/composebox/composebox_voice_search_recognition_test.js",
      "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsComposeboxTest, ComposeboxVoiceSearchFlags) {
  auto config = RegisterComposeboxTestConfig();
  RunTest("cr_components/composebox/composebox_voice_search_flags_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsComposeboxTest, ComposeboxDragAndDrop) {
  auto config = RegisterComposeboxTestConfig();
  RunTest("cr_components/composebox/composebox_drag_drop_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsComposeboxTest, ComposeboxErrorScrim) {
  auto config = RegisterComposeboxTestConfig();
  RunTest("cr_components/composebox/error_scrim_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsComposeboxTest, ComposeboxFileCarousel) {
  auto config = RegisterComposeboxTestConfig();
  RunTest("cr_components/composebox/file_carousel_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsComposeboxTest, ComposeboxFileThumbnail) {
  auto config = RegisterComposeboxTestConfig();
  RunTest("cr_components/composebox/file_thumbnail_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsComposeboxTest, ComposeboxFileInputs) {
  auto config = RegisterComposeboxTestConfig();
  RunTest("cr_components/composebox/composebox_file_inputs_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsComposeboxTest, ComposeboxInput) {
  auto config = RegisterComposeboxTestConfig();
  RunTest("cr_components/composebox/composebox_input_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsComposeboxTest, ComposeboxInputPlaceholder) {
  auto config = RegisterComposeboxTestConfig();
  RunTest("cr_components/composebox/composebox_input_placeholder_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsComposeboxTest, ComposeboxAutocomplete) {
  auto config = RegisterComposeboxTestConfig();
  RunTest("cr_components/composebox/composebox_autocomplete_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsComposeboxTest, ComposeboxDropdown) {
  auto config = RegisterComposeboxTestConfig();
  RunTest("cr_components/composebox/composebox_dropdown_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsComposeboxTest, ComposeboxMatch) {
  auto config = RegisterComposeboxTestConfig();
  RunTest("cr_components/composebox/composebox_match_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsComposeboxTest, ComposeboxMixin) {
  auto config = RegisterComposeboxTestConfig();
  RunTest("cr_components/composebox/composebox_mixin_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsComposeboxTest, ComposeboxAimButton) {
  RunTest("cr_components/composebox/composebox_aim_button_test.js",
          "runMochaSuite('ComposeboxAimButtonTest')");
}

#if !BUILDFLAG(IS_ANDROID)
class CrComponentsWithPixelOutputTest : public WebUIMochaBrowserTest {
 public:
  CrComponentsWithPixelOutputTest() {
    set_test_loader_host(chrome::kChromeUISettingsHost);
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(::switches::kEnablePixelOutputInTests);
    WebUIMochaBrowserTest::SetUpCommandLine(command_line);
  }
};

IN_PROC_BROWSER_TEST_F(CrComponentsWithPixelOutputTest, CrLottie) {
  RunTest("cr_components/cr_lottie/cr_lottie_test.js", "mocha.run()");
}
#endif  // !BUILDFLAG(IS_ANDROID)
