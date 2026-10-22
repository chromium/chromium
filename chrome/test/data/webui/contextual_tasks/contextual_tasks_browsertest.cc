// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/contextual_tasks/public/features.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "content/public/test/browser_test.h"
#include "extensions/buildflags/buildflags.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/test/base/chrome_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/mojom/css/preferred_color_scheme.mojom.h"
#endif

#if !BUILDFLAG(IS_ANDROID) && BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include <string>
#include <utility>

#include "base/strings/string_util.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/test/url_loader_interceptor.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_features.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/ui_base_switches.h"
#include "ui/compositor/compositor_switches.h"
#endif

class ContextualTasksBrowserTest : public WebUIMochaBrowserTest {
 protected:
  ContextualTasksBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {contextual_tasks::kContextualTasks,
         contextual_tasks::kContextualTasksForceEntryPointEligibility},
        {});
    set_test_loader_host(chrome::kChromeUIContextualTasksHost);
  }

  void SetUpInProcessBrowserTestFixture() override {
    WebUIMochaBrowserTest::SetUpInProcessBrowserTestFixture();
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &ContextualTasksBrowserTest::OnWillCreateBrowserContextServices,
                base::Unretained(this)));
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    AimEligibilityServiceFactory::GetInstance()->SetTestingFactory(
        context,
        base::BindRepeating(
            &ContextualTasksBrowserTest::BuildMockAimEligibilityService));
  }

  static std::unique_ptr<KeyedService> BuildMockAimEligibilityService(
      content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);
    auto aim_eligibility_service =
        std::make_unique<testing::NiceMock<MockAimEligibilityService>>(
            *profile->GetPrefs(), /*template_url_service=*/nullptr,
            /*url_loader_factory=*/nullptr, /*identity_manager=*/nullptr);

    ON_CALL(*aim_eligibility_service, IsAimEligible())
        .WillByDefault(testing::Return(true));
    ON_CALL(*aim_eligibility_service, IsCobrowseEligible())
        .WillByDefault(testing::Return(true));
    return aim_eligibility_service;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::CallbackListSubscription create_services_subscription_;
};

// TODO(crbug.com/487147580): Re-enable the test
IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, DISABLED_App) {
  RunTest("contextual_tasks/app_test.js", "mocha.run();");
}

// TODO(crbug.com/565086795): Flaky on Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_App_TracksFinishedTopLevelNavigation \
  DISABLED_App_TracksFinishedTopLevelNavigation
#define MAYBE_App_TracksFinishedTopLevelNavigationRace \
  DISABLED_App_TracksFinishedTopLevelNavigationRace
#else
#define MAYBE_App_TracksFinishedTopLevelNavigation \
  App_TracksFinishedTopLevelNavigation
#define MAYBE_App_TracksFinishedTopLevelNavigationRace \
  App_TracksFinishedTopLevelNavigationRace
#endif
IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       MAYBE_App_TracksFinishedTopLevelNavigation) {
  RunTest("contextual_tasks/app_test.js",
          "runMochaTest('ContextualTasksAppTest', "
          "'tracks finished top level navigation')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       MAYBE_App_TracksFinishedTopLevelNavigationRace) {
  RunTest(
      "contextual_tasks/app_test.js",
      "runMochaTest('ContextualTasksAppTest', "
      "'tracks finished top level navigation when content load wins race')");
}

#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       App_SidePanelZeroStateVisibleBeforeDomContentLoads) {
  RunTest("contextual_tasks/app_test.js",
          "runMochaTest('ContextualTasksAppTest', "
          "'side panel zero state is visible before DOM content loads')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       App_NewThreadClickWhenThreadFrameSrcIsEmpty) {
  RunTest("contextual_tasks/app_test.js",
          "runMochaTest('ContextualTasksAppTest', "
          "'new thread click when threadFrame src is empty does not throw')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, App_Composebox_BasicMode) {
  RunTest("contextual_tasks/app_composebox_basic_mode_test.js", "mocha.run();");
}

// TODO(crbug.com/480689282): Re-enable the test.
// TODO(crbug.com/527559266): Flaky on ChromeOS.
IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, DISABLED_Composebox) {
  RunTest("contextual_tasks/composebox_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, Composebox_Smoke) {
  RunTest("contextual_tasks/composebox_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkSmokeTest')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       Composebox_HiddenDuringNavigationToZeroState) {
  RunTest("contextual_tasks/composebox_test.js",
          "runMochaTest('ContextualTasksComposeboxTest', "
          "'composebox is hidden during subsequent navigation to zero-state "
          "until DOM loads')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, Composebox_BasicInput) {
  RunTest("contextual_tasks/composebox_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkBasicInputTest')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, Composebox_Dropdown) {
  RunTest("contextual_tasks/composebox_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkDropdownTest')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, Composebox_ContextMenu) {
  RunTest("contextual_tasks/composebox_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkContextMenuTest')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, Composebox_Files) {
  RunTest("contextual_tasks/composebox_files_test.js",
          "runMochaSuite('ContextualTasksComposeboxFilesTest')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       Composebox_Files_LensBehavior) {
  RunTest("contextual_tasks/composebox_files_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkFilesTest "
          "LensBehavior')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       Composebox_Files_FileInputsAndUploads) {
  RunTest("contextual_tasks/composebox_files_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkFilesTest "
          "FileInputsAndUploads')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       Composebox_Files_PlaceholderHints) {
  RunTest("contextual_tasks/composebox_files_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkFilesTest "
          "PlaceholderHints')");
}

// Run each AutoTab nested suite as a separate test so each group has its own
// per-test time budget.
IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       Composebox_AutoTab_ChipCreationAndMismatch) {
  RunTest("contextual_tasks/composebox_files_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkAutoTabTest "
          "ChipCreationAndMismatch')");
}

// TODO(crbug.com/556296442): Flaky on Linux.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_Composebox_AutoTab_DeletionSemantics \
  DISABLED_Composebox_AutoTab_DeletionSemantics
#else
#define MAYBE_Composebox_AutoTab_DeletionSemantics \
  Composebox_AutoTab_DeletionSemantics
#endif
IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       MAYBE_Composebox_AutoTab_DeletionSemantics) {
  RunTest("contextual_tasks/composebox_files_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkAutoTabTest "
          "DeletionSemantics')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       Composebox_AutoTab_SourcesAndUploadTiming) {
  RunTest("contextual_tasks/composebox_files_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkAutoTabTest "
          "SourcesAndUploadTiming')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       Composebox_AutoTab_UserActionsAndLifecycle) {
  RunTest("contextual_tasks/composebox_files_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkAutoTabTest "
          "UserActionsAndLifecycle')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       Composebox_AutoTab_SmartTabSharing) {
  RunTest("contextual_tasks/composebox_files_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkAutoTabTest "
          "SmartTabSharing')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, Composebox_Resize) {
  RunTest("contextual_tasks/composebox_test.js",
          "runMochaSuite('ContextualTasksComposeboxResizeTest')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, Composebox_Glow) {
  RunTest("contextual_tasks/composebox_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkGlowTest')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, Composebox_ErrorScrim) {
  RunTest("contextual_tasks/composebox_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkErrorScrimTest')");
}

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       ExtensionPostMessageHandler) {
  RunTest("contextual_tasks/extension_post_message_handler_test.js",
          "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, LensButton) {
  RunTest("contextual_tasks/lens_button_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, LensChip) {
  RunTest("contextual_tasks/lens_chip_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, TabPicker) {
  RunTest("contextual_tasks/tab_picker_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, ContextLibrary) {
  RunTest("contextual_tasks/context_library_test.js", "mocha.run();");
}

class ContextualTasksForcedDarkModeBrowserTest
    : public ContextualTasksBrowserTest {
 protected:
  ContextualTasksForcedDarkModeBrowserTest() {
    extensions::ComponentLoader::EnableBackgroundExtensionsForTesting();
    extension_feature_list_.InitWithFeatures(
        {contextual_tasks::kContextualTasksRearchitecture,
         contextual_tasks::kContextualTasksSidePanelRearchitecture,
         extensions_features::kApiContextualTasksPrivate},
        {});
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContextualTasksBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kForceDarkMode);
    command_line->AppendSwitch(switches::kEnablePixelOutputInTests);
  }

 private:
  base::test::ScopedFeatureList extension_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ContextualTasksForcedDarkModeBrowserTest, LensButton) {
  RunTest("contextual_tasks/lens_button_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksForcedDarkModeBrowserTest, LensChip) {
  RunTest("contextual_tasks/lens_chip_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksForcedDarkModeBrowserTest, TabPicker) {
  RunTest("contextual_tasks/tab_picker_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksForcedDarkModeBrowserTest,
                       ContextLibrary) {
  RunTest("contextual_tasks/context_library_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksForcedDarkModeBrowserTest,
    EmbeddedExtensionIframesStayTransparentAcrossColorSchemes) {
  constexpr char kHostPageUrl[] = "https://www.google.com/search?udm=50";
  // Google Search pages intentionally omit `<meta name="color-scheme">` and
  // CSS `color-scheme`, leaving each `<iframe>`'s owner color scheme as light.
  std::string host_page_html =
      "<!DOCTYPE html>"
      "<html><head><style>"
      "html, body { margin: 0; padding: 0; background: rgb(255, 255, 255); }"
      "#light-surface {"
      "  position: fixed; left: 0; top: 0; width: 120px; height: 120px;"
      "  background: rgb(255, 255, 255);"
      "}"
      "#dark-surface {"
      "  position: fixed; left: 140px; top: 0; width: 120px; height: 120px;"
      "  background: rgb(44, 46, 54);"
      "}"
      "iframe { border: 0; display: block; }"
      "</style></head><body>"
      "<div id='light-surface'>"
      "  <iframe id='light-tab-picker' width='120' height='120' "
      "src='chrome-extension://$EXT_ID/tab_picker.html?cs=0'></iframe>"
      "</div>"
      "<div id='dark-surface'>"
      "  <iframe id='dark-tab-picker' width='120' height='120' "
      "src='chrome-extension://$EXT_ID/tab_picker.html?cs=1'></iframe>"
      "</div>"
      "<div style='position: fixed; left: 0; top: 140px; "
      "background: rgb(255, 255, 255);'>"
      "  <iframe width='40' height='40' "
      "src='chrome-extension://$EXT_ID/context_library.html?cs=0'></iframe>"
      "  <iframe width='40' height='40' "
      "src='chrome-extension://$EXT_ID/lens_button.html?cs=0'></iframe>"
      "  <iframe width='40' height='40' "
      "src='chrome-extension://$EXT_ID/lens_chip.html?cs=0'></iframe>"
      "</div>"
      "<div style='position: fixed; left: 140px; top: 140px; "
      "background: rgb(44, 46, 54);'>"
      "  <iframe width='40' height='40' "
      "src='chrome-extension://$EXT_ID/context_library.html?cs=1'></iframe>"
      "  <iframe width='40' height='40' "
      "src='chrome-extension://$EXT_ID/lens_button.html?cs=1'></iframe>"
      "  <iframe width='40' height='40' "
      "src='chrome-extension://$EXT_ID/lens_chip.html?cs=1'></iframe>"
      "</div>"
      "</body></html>";
  base::ReplaceSubstringsAfterOffset(
      &host_page_html, 0, "$EXT_ID",
      extension_misc::kContextualTasksExtensionId);

  content::URLLoaderInterceptor interceptor(base::BindRepeating(
      [](const std::string& expected_url, const std::string& html,
         content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url.spec() != expected_url) {
          return false;
        }
        content::URLLoaderInterceptor::WriteResponse(
            "HTTP/1.1 200 OK\nContent-Type: text/html; charset=utf-8\n\n", html,
            params->client.get());
        return true;
      },
      std::string(kHostPageUrl), std::move(host_page_html)));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kHostPageUrl)));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* main_rfh = web_contents->GetPrimaryMainFrame();

  struct FrameExpectation {
    size_t child_index;
    const char* tag_name;
    bool expect_dark_mode;
  };
  constexpr FrameExpectation kExpectations[] = {
      {0, "tab-picker-app", false},  {1, "tab-picker-app", true},
      {2, "context-library", false}, {3, "lens-button-app", false},
      {4, "lens-chip-app", false},   {5, "context-library", true},
      {6, "lens-button-app", true},  {7, "lens-chip-app", true},
  };

  for (const auto& expectation : kExpectations) {
    content::RenderFrameHost* child_rfh =
        content::ChildFrameAt(main_rfh, expectation.child_index);
    ASSERT_NE(child_rfh, nullptr);
    ASSERT_TRUE(content::WaitForRenderFrameReady(child_rfh));

    // Confirm Chrome's preferred color scheme is dark inside every OOPIF while
    // `<html>` keeps `color-scheme: normal` so Blink does not synthesize an
    // opaque #121212 color-adjust canvas background against the light owner.
    EXPECT_EQ(true, content::EvalJs(
                        child_rfh,
                        "window.matchMedia('(prefers-color-scheme: dark)')"
                        ".matches"));
    EXPECT_EQ("normal",
              content::EvalJs(
                  child_rfh,
                  "getComputedStyle(document.documentElement).colorScheme"));

    const std::string check_host_js = content::JsReplace(
        R"js(
          (async () => {
            await customElements.whenDefined($1);
            const el = document.querySelector($1);
            if ('updateComplete' in el) {
              await el.updateComplete;
            }
            return {
              hasDarkAttr: el.hasAttribute('dark-mode'),
              colorScheme: getComputedStyle(el).colorScheme,
            };
          })()
        )js",
        expectation.tag_name);
    content::EvalJsResult eval_result =
        content::EvalJs(child_rfh, check_host_js);
    const base::DictValue& result = eval_result.ExtractDict();
    EXPECT_EQ(expectation.expect_dark_mode, result.FindBool("hasDarkAttr"));
    EXPECT_EQ(expectation.expect_dark_mode ? "dark" : "light",
              *result.FindString("colorScheme"));
  }

  // Paint a sentinel strip at the bottom of both tab-picker OOPIFs so pixel
  // readback waits until Viz has composited both cross-process surfaces before
  // asserting transparency at the top of each OOPIF.
  constexpr SkColor kReadySentinelColor = SkColorSetRGB(0, 200, 83);
  constexpr char kAddSentinelJs[] = R"js(
    (() => {
      const marker = document.createElement('div');
      marker.style.cssText =
          'position:fixed;left:0;bottom:0;width:100%;height:20px;' +
          'background:rgb(0, 200, 83);pointer-events:none;';
      document.body.appendChild(marker);
      return true;
    })()
  )js";
  ASSERT_EQ(true, content::EvalJs(content::ChildFrameAt(main_rfh, 0),
                                  kAddSentinelJs));
  ASSERT_EQ(true, content::EvalJs(content::ChildFrameAt(main_rfh, 1),
                                  kAddSentinelJs));

  content::RenderWidgetHostView* view = main_rfh->GetView();
  ASSERT_NE(view, nullptr);
  const float scale = view->GetDeviceScaleFactor();
  const int light_x = base::ClampRound(60 * scale);
  const int dark_x = base::ClampRound(200 * scale);
  const int transparent_y = base::ClampRound(12 * scale);
  const int sentinel_y = base::ClampRound(110 * scale);

  SkBitmap snapshot;
  ASSERT_TRUE(base::test::RunUntil([&]() {
    base::test::TestFuture<const content::CopyFromSurfaceResult&> copy_future;
    view->CopyFromSurface(gfx::Rect(), gfx::Size(), base::TimeDelta(),
                          copy_future.GetCallback());
    const auto& copy_result = copy_future.Get();
    if (!copy_result.has_value()) {
      return false;
    }
    snapshot = copy_result->bitmap;
    return !snapshot.drawsNothing() && dark_x < snapshot.width() &&
           sentinel_y < snapshot.height() &&
           snapshot.getColor(light_x, sentinel_y) == kReadySentinelColor &&
           snapshot.getColor(dark_x, sentinel_y) == kReadySentinelColor;
  }));

  ASSERT_FALSE(snapshot.drawsNothing());
  ASSERT_EQ(kReadySentinelColor, snapshot.getColor(light_x, sentinel_y));
  ASSERT_EQ(kReadySentinelColor, snapshot.getColor(dark_x, sentinel_y));
  EXPECT_EQ(SkColorSetRGB(255, 255, 255),
            snapshot.getColor(light_x, transparent_y));
  EXPECT_EQ(SkColorSetRGB(44, 46, 54),
            snapshot.getColor(dark_x, transparent_y));
}
#endif

// TODO(crbug.com/480689282): Flaky on ChromeOS debug.
// TODO(crbug.com/487147580): Re-enable on Linux.
// TODO(crbug.com/490250939): Flaky elsewhere as well.
IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       DISABLED_Composebox_MiscInputs) {
  RunTest("contextual_tasks/composebox_misc_inputs_test.js", "mocha.run();");
}

// TODO(crbug.com/487147580): Re-enable the test
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_Composebox_Submit DISABLED_Composebox_Submit
#else
#define MAYBE_Composebox_Submit Composebox_Submit
#endif
IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, MAYBE_Composebox_Submit) {
  RunTest("contextual_tasks/composebox_submit_test.js", "mocha.run();");
}

// TODO(crbug.com/480689282): Flaky on Linux and ChromeOS.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_Composebox_ZeroState DISABLED_Composebox_ZeroState
#else
#define MAYBE_Composebox_ZeroState Composebox_ZeroState
#endif
IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, MAYBE_Composebox_ZeroState) {
  RunTest("contextual_tasks/composebox_zero_state_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, UnboundedMenu) {
  RunTest("contextual_tasks/unbounded_menu_test.js", "mocha.run();");
}

// TODO(crbug.com/564474841): Flaky on Linux-js coverage.
// Disabled on Linux after timing out during JS coverage collection, after all
// Mocha assertions passed.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_Composebox_Voice_SurfaceAndStartup_CoherenceTrue \
  DISABLED_Composebox_Voice_SurfaceAndStartup_CoherenceTrue
#define MAYBE_Composebox_Voice_SurfaceAndStartup_CoherenceFalse \
  DISABLED_Composebox_Voice_SurfaceAndStartup_CoherenceFalse
#else
#define MAYBE_Composebox_Voice_SurfaceAndStartup_CoherenceTrue \
  Composebox_Voice_SurfaceAndStartup_CoherenceTrue
#define MAYBE_Composebox_Voice_SurfaceAndStartup_CoherenceFalse \
  Composebox_Voice_SurfaceAndStartup_CoherenceFalse
#endif
IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       MAYBE_Composebox_Voice_SurfaceAndStartup_CoherenceTrue) {
  RunTest("contextual_tasks/composebox_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkVoiceTest "
          "\\\\(coherence = true\\\\) SurfaceAndStartup')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       MAYBE_Composebox_Voice_SurfaceAndStartup_CoherenceFalse) {
  RunTest("contextual_tasks/composebox_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkVoiceTest "
          "\\\\(coherence = false\\\\) SurfaceAndStartup')");
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksBrowserTest,
    Composebox_Voice_RecognitionAndSubmission_CoherenceTrue) {
  RunTest("contextual_tasks/composebox_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkVoiceTest "
          "\\\\(coherence = true\\\\) RecognitionAndSubmission')");
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksBrowserTest,
    Composebox_Voice_RecognitionAndSubmission_CoherenceFalse) {
  RunTest("contextual_tasks/composebox_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkVoiceTest "
          "\\\\(coherence = false\\\\) RecognitionAndSubmission')");
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksBrowserTest,
    Composebox_Voice_ErrorPermissionAndLayout_CoherenceTrue) {
  RunTest("contextual_tasks/composebox_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkVoiceTest "
          "\\\\(coherence = true\\\\) ErrorPermissionAndLayout')");
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksBrowserTest,
    Composebox_Voice_ErrorPermissionAndLayout_CoherenceFalse) {
  RunTest("contextual_tasks/composebox_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkVoiceTest "
          "\\\\(coherence = false\\\\) ErrorPermissionAndLayout')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       Composebox_Voice_NonCoherenceTranscriptAndCancel) {
  RunTest("contextual_tasks/composebox_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkVoiceTest "
          "\\\\(coherence = false\\\\) NonCoherenceTranscriptAndCancel')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       Composebox_Voice_CoherenceControlsFilesAndLifecycle) {
  RunTest("contextual_tasks/composebox_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkVoiceTest "
          "\\\\(coherence = true\\\\) CoherenceControlsFilesAndLifecycle')");
}
#endif  // !BUILDFLAG(IS_ANDROID)

// TODO(crbug.com/565086795): Flaky on Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_PostMessageHandler DISABLED_PostMessageHandler
#define MAYBE_TopToolbarTest DISABLED_TopToolbarTest
#define MAYBE_OverflowMenu DISABLED_OverflowMenu
#define MAYBE_Toolbar DISABLED_Toolbar
#define MAYBE_OnboardingTooltip DISABLED_OnboardingTooltip
#else
#define MAYBE_PostMessageHandler PostMessageHandler
#define MAYBE_TopToolbarTest TopToolbarTest
#define MAYBE_OverflowMenu OverflowMenu
#define MAYBE_Toolbar Toolbar
#define MAYBE_OnboardingTooltip OnboardingTooltip
#endif
IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, MAYBE_PostMessageHandler) {
  RunTest("contextual_tasks/post_message_handler_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, MAYBE_TopToolbarTest) {
  RunTest("contextual_tasks/top_toolbar_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, MAYBE_OverflowMenu) {
  RunTest("contextual_tasks/overflow_menu_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, MAYBE_Toolbar) {
  RunTest("contextual_tasks/toolbar_test.js", "mocha.run();");
}
IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, MAYBE_OnboardingTooltip) {
  RunTest("contextual_tasks/onboarding_tooltip_test.js", "mocha.run();");
}

#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, InfoTooltip) {
  RunTest("contextual_tasks/info_tooltip_test.js", "mocha.run();");
}
// TODO(crbug.com/529817776): Re-enable when the timeouts get fixed.
#if BUILDFLAG(IS_LINUX) && !defined(NDEBUG)
#define MAYBE_WebView DISABLED_WebView
#else
#define MAYBE_WebView WebView
#endif
IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, MAYBE_WebView) {
  RunTest("contextual_tasks/contextual_tasks_webview_browsertest.js",
          "mocha.run();");
}

class ContextualTasksToolbarThemeBrowserTest
    : public ContextualTasksBrowserTest {
 protected:
  ContextualTasksToolbarThemeBrowserTest() {
    rearch_feature_list_.InitAndEnableFeature(
        contextual_tasks::kContextualTasksSidePanelRearchitecture);
  }

  void SetPreferredColorScheme(blink::mojom::PreferredColorScheme scheme) {
    content::WebContents* wc = chrome_test_utils::GetActiveWebContents(this);
    blink::web_pref::WebPreferences prefs = wc->GetOrCreateWebPreferences();
    prefs.preferred_color_scheme = scheme;
    wc->SetWebPreferences(prefs);
  }

 private:
  base::test::ScopedFeatureList rearch_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ContextualTasksToolbarThemeBrowserTest,
                       FollowsPreferredColorSchemeViaCss) {
  content::WebContents* wc = chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(content::NavigateToURL(
      wc, GURL(chrome::kChromeUIContextualTasksToolbarURL)));

  SetPreferredColorScheme(blink::mojom::PreferredColorScheme::kLight);
  EXPECT_EQ("light", content::EvalJs(wc, R"(
    (async () => {
      const app = document.querySelector('contextual-tasks-toolbar-app');
      await app.updateComplete;
      const toolbar = app.shadowRoot.querySelector('top-toolbar');
      await toolbar.updateComplete;
      const lightLogo = toolbar.shadowRoot.querySelector('.chrome-logo-light');
      const darkLogo = toolbar.shadowRoot.querySelector('.chrome-logo-dark');
      if (getComputedStyle(app).backgroundColor !== 'rgb(255, 255, 255)') {
        return 'bad bg: ' + getComputedStyle(app).backgroundColor;
      }
      const iconColor = getComputedStyle(app)
          .getPropertyValue('--toolbar-button-icon-color').replace(/\s+/g, '');
      if (iconColor !== 'rgb(71,71,71)') {
        return 'bad icon: ' + iconColor;
      }
      if (lightLogo && darkLogo) {
        if (getComputedStyle(lightLogo).display !== 'block' ||
            getComputedStyle(darkLogo).display !== 'none') {
          return 'bad logo display';
        }
      }
      return 'light';
    })();
  )"));

  SetPreferredColorScheme(blink::mojom::PreferredColorScheme::kDark);
  EXPECT_EQ("dark", content::EvalJs(wc, R"(
    (async () => {
      const app = document.querySelector('contextual-tasks-toolbar-app');
      await app.updateComplete;
      const toolbar = app.shadowRoot.querySelector('top-toolbar');
      await toolbar.updateComplete;
      const lightLogo = toolbar.shadowRoot.querySelector('.chrome-logo-light');
      const darkLogo = toolbar.shadowRoot.querySelector('.chrome-logo-dark');
      if (getComputedStyle(app).backgroundColor !== 'rgb(34, 36, 42)') {
        return 'bad bg: ' + getComputedStyle(app).backgroundColor;
      }
      const iconColor = getComputedStyle(app)
          .getPropertyValue('--toolbar-button-icon-color').replace(/\s+/g, '');
      if (iconColor !== 'rgb(199,199,199)') {
        return 'bad icon: ' + iconColor;
      }
      if (lightLogo && darkLogo) {
        if (getComputedStyle(lightLogo).display !== 'none' ||
            getComputedStyle(darkLogo).display !== 'block') {
          return 'bad logo display';
        }
      }
      return 'dark';
    })();
  )"));

  SetPreferredColorScheme(blink::mojom::PreferredColorScheme::kLight);
  EXPECT_EQ("light", content::EvalJs(wc, R"(
    getComputedStyle(document.querySelector('contextual-tasks-toolbar-app'))
        .backgroundColor === 'rgb(255, 255, 255)' ? 'light' : 'mismatch'
  )"));
}
#endif

// TODO(crbug.com/565086795): Flaky on Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_ClipPath DISABLED_ClipPath
#define MAYBE_WindowManager DISABLED_WindowManager
#define MAYBE_Utils DISABLED_Utils
#else
#define MAYBE_ClipPath ClipPath
#define MAYBE_WindowManager WindowManager
#define MAYBE_Utils Utils
#endif
IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, MAYBE_ClipPath) {
  RunTest("contextual_tasks/utils/clip_path_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, MAYBE_WindowManager) {
  RunTest("contextual_tasks/window_manager_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, MAYBE_Utils) {
  RunTest("contextual_tasks/utils_test.js", "mocha.run();");
}
