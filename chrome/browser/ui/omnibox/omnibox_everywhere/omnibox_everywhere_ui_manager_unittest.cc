// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_ui_manager.h"

#include <memory>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

#include "base/base_paths.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/autocomplete/chrome_aim_eligibility_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/new_tab_page/prefs/ntp_pref_names.h"
#include "chrome/browser/search_engines/ai_mode_button_service_factory.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_prefs.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_region_select_overlay.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_widget_delegate.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere_service.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere_service_factory.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/keep_alive_registry/keep_alive_registry.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/ntp_tiles/pref_names.h"
#include "components/omnibox/browser/omnibox_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/search_engines/test_ai_mode_button_service.h"
#include "content/public/browser/context_menu_params.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/context_menu_data/edit_flags.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/display/screen.h"
#include "ui/display/test/test_screen.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/menu_runner_handler.h"
#include "ui/views/test/menu_runner_test_api.h"
#include "ui/views/test/widget_activation_waiter.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
// clang-format off
#include <windows.h>
#include <shlobj.h>  // Must be before propkey.
// clang-format on

#include <propkey.h>
#include <propsys.h>
#include <shellapi.h>
#include <wrl/client.h>

#include "base/files/file_util.h"
#include "base/win/scoped_propvariant.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/views/win/hwnd_util.h"
#endif

namespace omnibox_everywhere {

namespace {

class ScopedScreenOverride {
 public:
  explicit ScopedScreenOverride(display::Screen* new_screen)
      : old_screen_(display::Screen::SetScreenInstance(nullptr)) {
    display::Screen::SetScreenInstance(new_screen);
  }
  ~ScopedScreenOverride() {
    display::Screen::SetScreenInstance(nullptr);
    if (old_screen_) {
      display::Screen::SetScreenInstance(old_screen_);
    }
  }

 private:
  raw_ptr<display::Screen> old_screen_;
};

class TestWebUIContentsWrapper : public WebUIContentsWrapper {
 public:
  explicit TestWebUIContentsWrapper(Profile* profile)
      : WebUIContentsWrapper(GURL(""), profile, 0, true, true, true, "Test") {}
  ~TestWebUIContentsWrapper() override = default;

  void ReloadWebContents() override {}

  base::WeakPtr<WebUIContentsWrapper> GetWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<TestWebUIContentsWrapper> weak_ptr_factory_{this};
};

class TestMenuRunnerHandler : public views::MenuRunnerHandler {
 public:
  TestMenuRunnerHandler() = default;
  ~TestMenuRunnerHandler() override = default;

  void RunMenuAt(views::Widget* parent,
                 views::MenuButtonController* button_controller,
                 const gfx::Rect& bounds,
                 views::MenuAnchorPosition anchor,
                 ui::mojom::MenuSourceType source_type,
                 int32_t types) override {}
};

class TestingAimEligibilityService : public ChromeAimEligibilityService {
 public:
  TestingAimEligibilityService(Profile* profile, bool is_fusebox_eligible)
      : ChromeAimEligibilityService(*profile->GetPrefs(),
                                    /*template_url_service=*/nullptr,
                                    /*url_loader_factory=*/nullptr,
                                    /*identity_manager=*/nullptr,
                                    /*configuration=*/{}),
        is_fusebox_eligible_(is_fusebox_eligible) {}

  variations::VariationsService* GetVariationsService() const override {
    return nullptr;
  }

  bool IsAimEligible() const override { return is_fusebox_eligible_; }
  bool IsFuseboxEligible() const override { return is_fusebox_eligible_; }
  bool IsAimAllowedByDse() const override { return is_fusebox_eligible_; }

 private:
  const bool is_fusebox_eligible_;
};

class TestingOmniboxEverywhereService : public OmniboxEverywhereService {
 public:
  explicit TestingOmniboxEverywhereService(Profile* profile)
      : OmniboxEverywhereService(profile) {}
  ~TestingOmniboxEverywhereService() override = default;

  void MaybeShowLensPromo() override { ++maybe_show_lens_promo_count_; }
  void EndLensPromo() override { ++end_lens_promo_count_; }

  int maybe_show_lens_promo_count() const {
    return maybe_show_lens_promo_count_;
  }
  int end_lens_promo_count() const { return end_lens_promo_count_; }

 private:
  int maybe_show_lens_promo_count_ = 0;
  int end_lens_promo_count_ = 0;
};

}  // namespace

class OmniboxEverywhereUIManagerTest : public ChromeViewsTestBase {
 public:
  OmniboxEverywhereUIManagerTest() = default;
  ~OmniboxEverywhereUIManagerTest() override = default;

  void SetUp() override {
    feature_list_.InitAndEnableFeature(omnibox::kOmniboxEverywhere);
    set_native_widget_type(NativeWidgetType::kDesktop);
#if BUILDFLAG(IS_WIN)
    // Showing a persistent widget creates the Start Menu shortcut that the
    // Shell requires for taskbar pinning. Keep that out of the real profile.
    ASSERT_TRUE(temp_start_menu_dir_.CreateUniqueTempDir());
    start_menu_override_.emplace(base::DIR_START_MENU,
                                 temp_start_menu_dir_.GetPath());
#endif
    ChromeViewsTestBase::SetUp();
  }

  void SetUpAimEligibilityService(bool is_fusebox_eligible) {
    AimEligibilityServiceFactory::GetInstance()->SetTestingFactory(
        &profile_,
        base::BindRepeating(
            [](bool fusebox_eligible, content::BrowserContext* context)
                -> std::unique_ptr<KeyedService> {
              return std::make_unique<TestingAimEligibilityService>(
                  static_cast<TestingProfile*>(context), fusebox_eligible);
            },
            is_fusebox_eligible));
  }

  void SetUpAiModeButtonService() {
    AiModeButtonServiceFactory::GetInstance()->SetTestingFactory(
        &profile_, base::BindRepeating([](content::BrowserContext* context)
                                           -> std::unique_ptr<KeyedService> {
          auto service = std::make_unique<TestAiModeButtonService>(
              /*template_url_service=*/nullptr);
          AiModeButtonUiConfig test_config(
              SearchEngineType::SEARCH_ENGINE_GOOGLE, u"AI Mode", u"Google",
              /*favicon_url=*/"", /*navigation_url=*/"",
              /*navigation_url_empty=*/"");
          service->current_ui_config_ = test_config;
          return service;
        }));
  }

  std::unique_ptr<OmniboxEverywhereUIManager> CreateUIManager() {
    auto ui_manager =
        std::make_unique<OmniboxEverywhereUIManager>(base::BindRepeating(
            [](Profile* profile) -> std::unique_ptr<WebUIContentsWrapper> {
              return std::make_unique<TestWebUIContentsWrapper>(profile);
            }));
    ui_manager->SetMenuRunnerFactoryForTesting(base::BindRepeating(
        [](ui::MenuModel* model, base::RepeatingClosure on_closed) {
          auto runner = std::make_unique<views::MenuRunner>(
              model,
              views::MenuRunner::HAS_MNEMONICS |
                  views::MenuRunner::CONTEXT_MENU,
              std::move(on_closed));
          views::test::MenuRunnerTestAPI(runner.get())
              .SetMenuRunnerHandler(std::make_unique<TestMenuRunnerHandler>());
          return runner;
        }));
    return ui_manager;
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  TestingProfile profile_;
#if BUILDFLAG(IS_WIN)
  base::ScopedTempDir temp_start_menu_dir_;
  std::optional<base::ScopedPathOverride> start_menu_override_;
#endif
};

TEST_F(OmniboxEverywhereUIManagerTest, ShowAndCloseWidget) {
  auto ui_manager = CreateUIManager();

  // Initially, no widget should exist.
  EXPECT_FALSE(ui_manager->widget());

  // Showing the UI manager for a profile should instantiate and display a
  // widget.
  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* widget = ui_manager->widget();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());

  // Closing the UI manager should trigger hiding the widget.
  ui_manager->Close();
  EXPECT_FALSE(widget->IsVisible());
  EXPECT_TRUE(ui_manager->widget());

  // Shutdown should destroy the widget.
  ui_manager->Shutdown();
  EXPECT_FALSE(ui_manager->widget());
}

TEST_F(OmniboxEverywhereUIManagerTest, ShowWhileWidgetIsHidden) {
  auto ui_manager = CreateUIManager();

  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* first_widget = ui_manager->widget();
  ASSERT_TRUE(first_widget);

  // Close (hide) the widget.
  ui_manager->Close();
  EXPECT_FALSE(first_widget->IsVisible());

  // Showing it again immediately should reactivate the existing hidden widget.
  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* second_widget = ui_manager->widget();
  ASSERT_TRUE(second_widget);
  EXPECT_TRUE(second_widget->IsVisible());
  EXPECT_EQ(first_widget, second_widget);

  // Clean up.
  ui_manager->Shutdown();
  EXPECT_FALSE(ui_manager->widget());
}

// Verifies that when the widget is first shown on a standard display, its
// initial bounds are centered and initialized with the default resting height
// (kDefaultRestingHeight) and fixed width (kPopupFixedWidth) to prevent visual
// resize flashing before WebUI auto-resize occurs.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_InitialBoundsMatchRestingHeight \
  DISABLED_InitialBoundsMatchRestingHeight
#else
#define MAYBE_InitialBoundsMatchRestingHeight InitialBoundsMatchRestingHeight
#endif
TEST_F(OmniboxEverywhereUIManagerTest, MAYBE_InitialBoundsMatchRestingHeight) {
  profile_.GetPrefs()->SetBoolean(omnibox_everywhere::prefs::kFreDismissed,
                                  true);
  display::test::TestScreen test_screen(/*create_display=*/false,
                                        /*register_screen=*/false);
  ScopedScreenOverride screen_override(&test_screen);

  display::Display display1(1, gfx::Rect(0, 0, 1920, 1080));
  test_screen.display_list().AddDisplay(display1,
                                        display::DisplayList::Type::PRIMARY);

  auto ui_manager = CreateUIManager();
  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* widget = ui_manager->widget();
  ASSERT_TRUE(widget);

  const int expected_width =
      omnibox_everywhere::OmniboxEverywhereUIManager::GetPopupFixedWidth();
  const int expected_x = (1920 - expected_width) / 2;
  EXPECT_EQ(widget->GetWindowBoundsInScreen(),
            gfx::Rect(expected_x, 464, expected_width,
                      omnibox_everywhere::OmniboxEverywhereUIManager::
                          kDefaultRestingHeight));

  ui_manager->Shutdown();
}

#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_InitialBoundsMatchFreModalRestingHeight \
  DISABLED_InitialBoundsMatchFreModalRestingHeight
#else
#define MAYBE_InitialBoundsMatchFreModalRestingHeight \
  InitialBoundsMatchFreModalRestingHeight
#endif
TEST_F(OmniboxEverywhereUIManagerTest,
       MAYBE_InitialBoundsMatchFreModalRestingHeight) {
  display::test::TestScreen test_screen(/*create_display=*/false,
                                        /*register_screen=*/false);
  ScopedScreenOverride screen_override(&test_screen);

  display::Display display1(1, gfx::Rect(0, 0, 1920, 1080));
  test_screen.display_list().AddDisplay(display1,
                                        display::DisplayList::Type::PRIMARY);

  const int expected_width =
      omnibox_everywhere::OmniboxEverywhereUIManager::GetPopupFixedWidth();
  const int expected_x = (1920 - expected_width) / 2;

  // 1. Fresh profile with kOmniboxEverywhereFre enabled starts at
  // FreStage::kIntroModal and uses kFreModalRestingHeight.
  ASSERT_EQ(omnibox_everywhere::prefs::GetCurrentFreStage(&profile_),
            omnibox_everywhere::prefs::FreStage::kIntroModal);
  {
    auto ui_manager = CreateUIManager();
    ui_manager->ShowForProfile(&profile_, GetContext());
    views::Widget* widget = ui_manager->widget();
    ASSERT_TRUE(widget);

    const int expected_fre_y =
        (1080 - omnibox_everywhere::OmniboxEverywhereUIManager::
                    kFreModalRestingHeight) /
        2;
    EXPECT_EQ(widget->GetWindowBoundsInScreen(),
              gfx::Rect(expected_x, expected_fre_y, expected_width,
                        omnibox_everywhere::OmniboxEverywhereUIManager::
                            kFreModalRestingHeight));
    ui_manager->Shutdown();
  }

  // 2. When FreStage transitions to kShortcutSetupChin (intro modal dismissed),
  // initial bounds use kDefaultRestingHeight.
  profile_.GetPrefs()->SetBoolean(omnibox_everywhere::prefs::kFreIntroDismissed,
                                  true);
  ASSERT_EQ(omnibox_everywhere::prefs::GetCurrentFreStage(&profile_),
            omnibox_everywhere::prefs::FreStage::kShortcutSetupChin);
  {
    auto ui_manager = CreateUIManager();
    ui_manager->ShowForProfile(&profile_, GetContext());
    views::Widget* widget = ui_manager->widget();
    ASSERT_TRUE(widget);

    EXPECT_EQ(widget->GetWindowBoundsInScreen(),
              gfx::Rect(expected_x, 464, expected_width,
                        omnibox_everywhere::OmniboxEverywhereUIManager::
                            kDefaultRestingHeight));
    ui_manager->Shutdown();
  }

  // 3. When kOmniboxEverywhereFre feature is disabled on a fresh profile,
  // initial bounds use kDefaultRestingHeight.
  profile_.GetPrefs()->SetBoolean(omnibox_everywhere::prefs::kFreIntroDismissed,
                                  false);
  base::test::ScopedFeatureList disable_fre;
  disable_fre.InitAndDisableFeature(omnibox::kOmniboxEverywhereFre);
  {
    auto ui_manager = CreateUIManager();
    ui_manager->ShowForProfile(&profile_, GetContext());
    views::Widget* widget = ui_manager->widget();
    ASSERT_TRUE(widget);

    EXPECT_EQ(widget->GetWindowBoundsInScreen(),
              gfx::Rect(expected_x, 464, expected_width,
                        omnibox_everywhere::OmniboxEverywhereUIManager::
                            kDefaultRestingHeight));
    ui_manager->Shutdown();
  }
}

TEST_F(OmniboxEverywhereUIManagerTest, SmallLoomniboxBounds) {
  profile_.GetPrefs()->SetBoolean(omnibox_everywhere::prefs::kFreDismissed,
                                  true);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      omnibox::kOmniboxEverywhere, {{"smallLoomnibox", "true"}});

  EXPECT_EQ(
      omnibox_everywhere::OmniboxEverywhereUIManager::GetPopupFixedWidth(),
      omnibox_everywhere::OmniboxEverywhereUIManager::kPopupSmallFixedWidth);

  display::test::TestScreen test_screen(/*create_display=*/false,
                                        /*register_screen=*/false);
  ScopedScreenOverride screen_override(&test_screen);

  display::Display display1(1, gfx::Rect(0, 0, 1920, 1080));
  test_screen.display_list().AddDisplay(display1,
                                        display::DisplayList::Type::PRIMARY);

  auto ui_manager = CreateUIManager();
  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* widget = ui_manager->widget();
  ASSERT_TRUE(widget);

  EXPECT_EQ(
      widget->GetWindowBoundsInScreen(),
      gfx::Rect(
          696, 464,
          omnibox_everywhere::OmniboxEverywhereUIManager::kPopupSmallFixedWidth,
          omnibox_everywhere::OmniboxEverywhereUIManager::
              kDefaultRestingHeight));

  ui_manager->Shutdown();
}

// Verifies that on displays with dimensions smaller than the fixed popup width,
// widget bounds calculation clamps width and coordinates to remain fully within
// the visible work area without overflowing or negative positioning.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_CalculateBoundsClampsToSmallDisplays \
  DISABLED_CalculateBoundsClampsToSmallDisplays
#else
#define MAYBE_CalculateBoundsClampsToSmallDisplays \
  CalculateBoundsClampsToSmallDisplays
#endif
TEST_F(OmniboxEverywhereUIManagerTest,
       MAYBE_CalculateBoundsClampsToSmallDisplays) {
  profile_.GetPrefs()->SetBoolean(omnibox_everywhere::prefs::kFreDismissed,
                                  true);
  display::test::TestScreen test_screen(/*create_display=*/false,
                                        /*register_screen=*/false);
  ScopedScreenOverride screen_override(&test_screen);

  // Display smaller than default popup width.
  const int popup_width =
      omnibox_everywhere::OmniboxEverywhereUIManager::GetPopupFixedWidth();
  const int small_display_width = popup_width - 28;
  display::Display small_display(1, gfx::Rect(0, 0, small_display_width, 600));
  test_screen.display_list().AddDisplay(small_display,
                                        display::DisplayList::Type::PRIMARY);

  auto ui_manager = CreateUIManager();
  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* widget = ui_manager->widget();
  ASSERT_TRUE(widget);

  // Width is clamped to work area width and x starts at 0 (non-negative).
  EXPECT_EQ(widget->GetWindowBoundsInScreen().x(), 0);
  EXPECT_EQ(widget->GetWindowBoundsInScreen().width(), small_display_width);
  EXPECT_EQ(
      widget->GetWindowBoundsInScreen().height(),
      omnibox_everywhere::OmniboxEverywhereUIManager::kDefaultRestingHeight);

  ui_manager->Shutdown();
}

TEST_F(OmniboxEverywhereUIManagerTest, FileChooserStateTracking) {
  auto ui_manager = CreateUIManager();
  ui_manager->ShowForProfile(&profile_, GetContext());
  ASSERT_TRUE(ui_manager->web_contents());

  EXPECT_FALSE(ui_manager->is_file_chooser_open_for_testing());
  EXPECT_FALSE(ui_manager->web_contents()->ShouldIgnoreInputEventsForTesting());

  ui_manager->OnFileChooserOpened();
  EXPECT_TRUE(ui_manager->is_file_chooser_open_for_testing());
  EXPECT_TRUE(ui_manager->web_contents()->ShouldIgnoreInputEventsForTesting());

  ui_manager->OnFileChooserClosed();
  EXPECT_FALSE(ui_manager->is_file_chooser_open_for_testing());
  EXPECT_FALSE(ui_manager->web_contents()->ShouldIgnoreInputEventsForTesting());
}

TEST_F(OmniboxEverywhereUIManagerTest, DismissOnDeactivationInEphemeralMode) {
  if (g_browser_process && g_browser_process->local_state()) {
    g_browser_process->local_state()->SetBoolean(
        omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel, true);
  }
  auto ui_manager = CreateUIManager();

  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* widget = ui_manager->widget();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());

  // Advance time past the activation grace period.
  task_environment()->FastForwardBy(
      omnibox_everywhere::OmniboxEverywhereUIManager::kActivationGracePeriod +
      base::Milliseconds(1));

  // Simulating deactivation (active = false) in ephemeral mode after the grace
  // period should hide the widget.
  ui_manager->OnWidgetActivationChanged(widget, /*active=*/false);
  EXPECT_TRUE(base::test::RunUntil([&]() { return !widget->IsVisible(); }));
  EXPECT_TRUE(ui_manager->widget());
}

TEST_F(OmniboxEverywhereUIManagerTest, DismissOnSpaceSwitchInEphemeralMode) {
  if (g_browser_process && g_browser_process->local_state()) {
    g_browser_process->local_state()->SetBoolean(
        omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel, true);
  }
  auto ui_manager = CreateUIManager();

  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* widget = ui_manager->widget();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());

  // Simulating the widget becoming invisible on screen (e.g., active Space
  // change on macOS) in ephemeral mode should close/hide the widget.
  ui_manager->OnWidgetVisibilityOnScreenChanged(widget, /*visible=*/false);
  EXPECT_FALSE(widget->IsVisible());
}

TEST_F(OmniboxEverywhereUIManagerTest, NoDismissOnSpaceSwitchInPersistentMode) {
  if (g_browser_process && g_browser_process->local_state()) {
    g_browser_process->local_state()->SetBoolean(
        omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel, false);
  }
  auto ui_manager = CreateUIManager();

  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* widget = ui_manager->widget();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());

  // In persistent mode, changing active space should not close/hide the widget.
  ui_manager->OnWidgetVisibilityOnScreenChanged(widget, /*visible=*/false);
  EXPECT_TRUE(widget->IsVisible());
}

TEST_F(OmniboxEverywhereUIManagerTest,
       DismissOnSpaceSwitchBypassedDuringModalDialog) {
  if (g_browser_process && g_browser_process->local_state()) {
    g_browser_process->local_state()->SetBoolean(
        omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel, true);
  }
  auto ui_manager = CreateUIManager();

  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* widget = ui_manager->widget();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());

  // Mark a modal dialog as open.
  ui_manager->OnPermissionPromptChanged(/*is_showing=*/true,
                                        gfx::Size(100, 100));
  EXPECT_TRUE(ui_manager->HasOpenModalDialog());

  // Becoming invisible on screen while a modal dialog is open should NOT close
  // the widget.
  ui_manager->OnWidgetVisibilityOnScreenChanged(widget, /*visible=*/false);
  EXPECT_TRUE(ui_manager->widget());
  EXPECT_TRUE(widget->IsVisible());
}

TEST_F(OmniboxEverywhereUIManagerTest,
       DismissOnSpaceSwitchBypassedDuringContextMenu) {
  if (g_browser_process && g_browser_process->local_state()) {
    g_browser_process->local_state()->SetBoolean(
        omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel, true);
  }
  auto ui_manager = CreateUIManager();

  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* widget = ui_manager->widget();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());

  // Simulate context menu open.
  ui_manager->set_is_context_menu_open_for_testing(true);
  EXPECT_TRUE(ui_manager->is_context_menu_open_for_testing());

  // Becoming invisible on screen while a context menu is open should NOT close
  // the widget.
  ui_manager->OnWidgetVisibilityOnScreenChanged(widget, /*visible=*/false);
  EXPECT_TRUE(ui_manager->widget());
  EXPECT_TRUE(widget->IsVisible());
}

// TODO(crbug.com/546604786): Deactivation within grace period tests are flaky
// on Linux due to lack of window manager activation synchronization in tests.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_DeactivationWithinGracePeriodReactivatesWidget \
  DISABLED_DeactivationWithinGracePeriodReactivatesWidget
#else
#define MAYBE_DeactivationWithinGracePeriodReactivatesWidget \
  DeactivationWithinGracePeriodReactivatesWidget
#endif
TEST_F(OmniboxEverywhereUIManagerTest,
       MAYBE_DeactivationWithinGracePeriodReactivatesWidget) {
  if (g_browser_process && g_browser_process->local_state()) {
    g_browser_process->local_state()->SetBoolean(
        omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel, true);
  }
  auto ui_manager = CreateUIManager();

  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* widget = ui_manager->widget();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());

  // Advance time within the grace period (e.g. 100ms < 500ms).
  task_environment()->FastForwardBy(base::Milliseconds(100));

  // Simulating deactivation (active = false) within the grace period should NOT
  // hide the widget, but instead reactivate it and keep it visible.
  ui_manager->OnWidgetActivationChanged(widget, /*active=*/false);
  EXPECT_TRUE(base::test::RunUntil([&]() { return widget->IsActive(); }));
  EXPECT_TRUE(widget->IsVisible());

  // Advance time past the grace period.
  task_environment()->FastForwardBy(
      omnibox_everywhere::OmniboxEverywhereUIManager::kActivationGracePeriod);

  // Deactivation after the grace period has elapsed should cleanly dismiss.
  ui_manager->OnWidgetActivationChanged(widget, /*active=*/false);
  EXPECT_TRUE(base::test::RunUntil([&]() { return !widget->IsVisible(); }));
}

// TODO(crbug.com/546604786): Explicit close within grace period tests are flaky
// on Linux due to lack of window manager activation synchronization in tests.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_ExplicitCloseWithinGracePeriodStaysClosed \
  DISABLED_ExplicitCloseWithinGracePeriodStaysClosed
#else
#define MAYBE_ExplicitCloseWithinGracePeriodStaysClosed \
  ExplicitCloseWithinGracePeriodStaysClosed
#endif
TEST_F(OmniboxEverywhereUIManagerTest,
       MAYBE_ExplicitCloseWithinGracePeriodStaysClosed) {
  if (g_browser_process && g_browser_process->local_state()) {
    g_browser_process->local_state()->SetBoolean(
        omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel, true);
  }
  auto ui_manager = CreateUIManager();

  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* widget = ui_manager->widget();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());

  // Advance time within the grace period (e.g. 100ms < 500ms).
  task_environment()->FastForwardBy(base::Milliseconds(100));

  // Simulate deactivation within the grace period, which schedules
  // reactivation.
  ui_manager->OnWidgetActivationChanged(widget, /*active=*/false);

  // Explicitly closing the widget (e.g. Esc key) within the grace period should
  // cancel the reactivation task and hide the widget.
  ui_manager->Close();
  EXPECT_FALSE(widget->IsVisible());
}

TEST_F(OmniboxEverywhereUIManagerTest,
       PersistentDeactivationDemotesWidgetAndKeepsVisible) {
  if (g_browser_process && g_browser_process->local_state()) {
    g_browser_process->local_state()->SetBoolean(
        omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel, false);
  }
  auto ui_manager = CreateUIManager();

  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* widget = ui_manager->widget();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());
  views::test::WaitForWidgetActive(widget, true);
  EXPECT_FALSE(ui_manager->is_demoted());
  EXPECT_TRUE(ui_manager->IsActive());
  EXPECT_EQ(widget->GetZOrderLevel(), ui::ZOrderLevel::kNormal);

  // Advance time past the activation grace period.
  task_environment()->FastForwardBy(
      omnibox_everywhere::OmniboxEverywhereUIManager::kActivationGracePeriod +
      base::Milliseconds(1));

  // Simulating deactivation (active = false) in persistent mode demotes the
  // widget (is_demoted_ == true) while keeping it visible on the desktop layer.
  ui_manager->OnWidgetActivationChanged(widget, /*active=*/false);
  EXPECT_TRUE(base::test::RunUntil([&]() { return ui_manager->is_demoted(); }));
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_FALSE(ui_manager->IsActive());
  EXPECT_EQ(widget->GetZOrderLevel(), ui::ZOrderLevel::kNormal);

  // Re-invoking ShowForProfile restores active state (clears demoted state).
  ui_manager->ShowForProfile(&profile_, GetContext());
  EXPECT_TRUE(widget->IsVisible());
  views::test::WaitForWidgetActive(widget, true);
  EXPECT_FALSE(ui_manager->is_demoted());
  EXPECT_TRUE(ui_manager->IsActive());
  EXPECT_EQ(widget->GetZOrderLevel(), ui::ZOrderLevel::kNormal);

  ui_manager->Close();
}

TEST_F(OmniboxEverywhereUIManagerTest,
       PersistentDeactivationWithinGracePeriodReactivatesWidget) {
  if (g_browser_process && g_browser_process->local_state()) {
    g_browser_process->local_state()->SetBoolean(
        omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel, false);
  }
  auto ui_manager = CreateUIManager();

  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* widget = ui_manager->widget();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());
  views::test::WaitForWidgetActive(widget, true);
  EXPECT_FALSE(ui_manager->is_demoted());
  EXPECT_TRUE(ui_manager->IsActive());

  // Advance time within the grace period (e.g. 100ms < 500ms).
  task_environment()->FastForwardBy(base::Milliseconds(100));

  // Simulating deactivation (active = false) within the grace period should NOT
  // demote the widget, but instead schedule reactivation and keep it visible.
  ui_manager->OnWidgetActivationChanged(widget, /*active=*/false);
  EXPECT_TRUE(base::test::RunUntil([&]() { return ui_manager->IsActive(); }));
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_FALSE(ui_manager->is_demoted());

  // Advance time past the grace period.
  task_environment()->FastForwardBy(
      omnibox_everywhere::OmniboxEverywhereUIManager::kActivationGracePeriod);

  // Deactivation after the grace period has elapsed should cleanly demote.
  ui_manager->OnWidgetActivationChanged(widget, /*active=*/false);
  EXPECT_TRUE(base::test::RunUntil([&]() { return ui_manager->is_demoted(); }));
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_FALSE(ui_manager->IsActive());

  ui_manager->Close();
}

TEST_F(OmniboxEverywhereUIManagerTest,
       PersistentDeactivationBypassedDuringFileChooser) {
  if (g_browser_process && g_browser_process->local_state()) {
    g_browser_process->local_state()->SetBoolean(
        omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel, false);
  }
  auto ui_manager = CreateUIManager();

  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* widget = ui_manager->widget();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());
  views::test::WaitForWidgetActive(widget, true);
  EXPECT_FALSE(ui_manager->is_demoted());
  EXPECT_TRUE(ui_manager->IsActive());

  task_environment()->FastForwardBy(
      omnibox_everywhere::OmniboxEverywhereUIManager::kActivationGracePeriod +
      base::Milliseconds(1));

  ui_manager->OnFileChooserOpened();
  EXPECT_TRUE(ui_manager->HasOpenModalDialog());

  // Simulating deactivation while file chooser is open should NOT demote.
  ui_manager->OnWidgetActivationChanged(widget, /*active=*/false);
  task_environment()->FastForwardBy(base::Milliseconds(100));
  EXPECT_FALSE(ui_manager->is_demoted());
  EXPECT_TRUE(ui_manager->IsActive());
  EXPECT_TRUE(widget->IsVisible());

  ui_manager->OnFileChooserClosed();
  EXPECT_FALSE(ui_manager->HasOpenModalDialog());

  // Deactivation after file chooser is closed should cleanly demote.
  ui_manager->OnWidgetActivationChanged(widget, /*active=*/false);
  EXPECT_TRUE(base::test::RunUntil([&]() { return ui_manager->is_demoted(); }));
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_FALSE(ui_manager->IsActive());

  ui_manager->Close();
}

// Tests that Demote() deactivates and keeps the widget visible.
#if BUILDFLAG(IS_WIN)
#define MAYBE_DemoteWidget DemoteWidget
#else
#define MAYBE_DemoteWidget DISABLED_DemoteWidget
#endif
TEST_F(OmniboxEverywhereUIManagerTest, MAYBE_DemoteWidget) {
  if (g_browser_process && g_browser_process->local_state()) {
    g_browser_process->local_state()->SetBoolean(
        omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel, false);
  }
  auto ui_manager = CreateUIManager();

  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* widget = ui_manager->widget();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());
  views::test::WaitForWidgetActive(widget, true);
  EXPECT_TRUE(ui_manager->IsActive());
  EXPECT_EQ(widget->GetZOrderLevel(), ui::ZOrderLevel::kNormal);

  // Demote() should deactivate and keep visible.
  ui_manager->Demote();
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_FALSE(ui_manager->IsActive());
  EXPECT_EQ(widget->GetZOrderLevel(), ui::ZOrderLevel::kNormal);

  // Calling Demote() again while already demoted is a safe no-op.
  ui_manager->Demote();
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_FALSE(ui_manager->IsActive());
  EXPECT_EQ(widget->GetZOrderLevel(), ui::ZOrderLevel::kNormal);

  // Calling ShowForProfile again activates.
  ui_manager->ShowForProfile(&profile_, GetContext());
  EXPECT_TRUE(widget->IsVisible());
  views::test::WaitForWidgetActive(widget, true);
  EXPECT_TRUE(ui_manager->IsActive());
  EXPECT_EQ(widget->GetZOrderLevel(), ui::ZOrderLevel::kNormal);

  ui_manager->Close();
}

// Tests that Demote() when OE is inactive does not deactivate other active
// windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_DemoteWhenInactiveDoesNotDeactivateOtherWidget \
  DemoteWhenInactiveDoesNotDeactivateOtherWidget
#else
#define MAYBE_DemoteWhenInactiveDoesNotDeactivateOtherWidget \
  DISABLED_DemoteWhenInactiveDoesNotDeactivateOtherWidget
#endif
TEST_F(OmniboxEverywhereUIManagerTest,
       MAYBE_DemoteWhenInactiveDoesNotDeactivateOtherWidget) {
  if (g_browser_process && g_browser_process->local_state()) {
    g_browser_process->local_state()->SetBoolean(
        omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel, false);
  }
  auto ui_manager = CreateUIManager();

  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* oe_widget = ui_manager->widget();
  ASSERT_TRUE(oe_widget);
  EXPECT_TRUE(oe_widget->IsVisible());
  views::test::WaitForWidgetActive(oe_widget, true);
  EXPECT_TRUE(ui_manager->IsActive());

  // Create and activate a separate widget.
  auto other_widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  other_widget->Show();
  views::test::WaitForWidgetActive(other_widget.get(), true);

  // OE widget is visible but inactive.
  EXPECT_TRUE(oe_widget->IsVisible());
  EXPECT_FALSE(oe_widget->IsActive());
  EXPECT_FALSE(ui_manager->IsActive());
  EXPECT_TRUE(other_widget->IsActive());

  // Demote() should demote OE without deactivating `other_widget`.
  ui_manager->Demote();
  EXPECT_TRUE(oe_widget->IsVisible());
  EXPECT_FALSE(ui_manager->IsActive());
  EXPECT_TRUE(other_widget->IsActive());

  ui_manager->Close();
}

// Tests that Demote() cancels any open context menu before demoting.
#if BUILDFLAG(IS_WIN)
#define MAYBE_DemoteCancelsOpenContextMenu DemoteCancelsOpenContextMenu
#else
#define MAYBE_DemoteCancelsOpenContextMenu DISABLED_DemoteCancelsOpenContextMenu
#endif
TEST_F(OmniboxEverywhereUIManagerTest, MAYBE_DemoteCancelsOpenContextMenu) {
  if (g_browser_process && g_browser_process->local_state()) {
    g_browser_process->local_state()->SetBoolean(
        omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel, false);
  }
  auto ui_manager = CreateUIManager();

  ui_manager->ShowForProfile(&profile_, GetContext());
  ASSERT_TRUE(ui_manager->widget());

  auto* rfh = ui_manager->contents_wrapper_for_testing()
                  ->web_contents()
                  ->GetPrimaryMainFrame();
  content::ContextMenuParams params;
  params.is_editable = true;

  bool menu_runner_created = false;
  ui_manager->SetMenuRunnerFactoryForTesting(base::BindRepeating(
      [](bool* created, ui::MenuModel* model,
         base::RepeatingClosure on_closed) {
        *created = true;
        auto runner = std::make_unique<views::MenuRunner>(
            model,
            views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU,
            on_closed);
        views::test::MenuRunnerTestAPI(runner.get())
            .SetMenuRunnerHandler(std::make_unique<TestMenuRunnerHandler>());
        return runner;
      },
      &menu_runner_created));

  ui_manager->HandleContextMenu(*rfh, params);
  EXPECT_TRUE(menu_runner_created);
  EXPECT_TRUE(ui_manager->is_context_menu_open_for_testing());

  // Demote() while context menu is open should cancel the runner and demote.
  ui_manager->Demote();
  EXPECT_FALSE(ui_manager->is_context_menu_open_for_testing());
  EXPECT_TRUE(ui_manager->widget()->IsVisible());
  EXPECT_EQ(ui_manager->widget()->GetZOrderLevel(), ui::ZOrderLevel::kNormal);

  ui_manager->Close();
}

// Tests that Demote() is a no-op while a file chooser is open.
#if BUILDFLAG(IS_WIN)
#define MAYBE_DemoteBypassedDuringFileChooser DemoteBypassedDuringFileChooser
#else
#define MAYBE_DemoteBypassedDuringFileChooser \
  DISABLED_DemoteBypassedDuringFileChooser
#endif
TEST_F(OmniboxEverywhereUIManagerTest, MAYBE_DemoteBypassedDuringFileChooser) {
  if (g_browser_process && g_browser_process->local_state()) {
    g_browser_process->local_state()->SetBoolean(
        omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel, false);
  }
  auto ui_manager = CreateUIManager();

  ui_manager->ShowForProfile(&profile_, GetContext());
  ASSERT_TRUE(ui_manager->widget());
  views::test::WaitForWidgetActive(ui_manager->widget(), true);

  ui_manager->OnFileChooserOpened();
  EXPECT_TRUE(ui_manager->HasOpenModalDialog());

  ui_manager->Demote();
  // Demote should have returned early, leaving widget active.
  EXPECT_TRUE(ui_manager->IsActive());

  ui_manager->OnFileChooserClosed();
  EXPECT_FALSE(ui_manager->HasOpenModalDialog());

  ui_manager->Close();
}

// Tests that closing a context menu in persistent mode leaves the widget
// visible.
TEST_F(OmniboxEverywhereUIManagerTest,
       ContextMenuClosedInPersistentModeDoesNotDismiss) {
  if (g_browser_process && g_browser_process->local_state()) {
    g_browser_process->local_state()->SetBoolean(
        omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel, false);
  }
  auto ui_manager = CreateUIManager();

  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* widget = ui_manager->widget();
  ASSERT_TRUE(widget);

  ui_manager->set_is_context_menu_open_for_testing(true);

  // Simulate deactivation while menu is open.
  ui_manager->OnWidgetActivationChanged(widget, /*active=*/false);
  EXPECT_TRUE(widget->IsVisible());

  // Closing context menu while inactive in persistent mode leaves widget
  // visible.
  ui_manager->OnContextMenuClosedForTesting();
  EXPECT_TRUE(widget->IsVisible());

  ui_manager->Close();
}

// Tests that CloseUI() demotes the widget in persistent mode.
#if BUILDFLAG(IS_WIN)
#define MAYBE_CloseUIInPersistentModeDemotesWidget \
  CloseUIInPersistentModeDemotesWidget
#else
#define MAYBE_CloseUIInPersistentModeDemotesWidget \
  DISABLED_CloseUIInPersistentModeDemotesWidget
#endif
TEST_F(OmniboxEverywhereUIManagerTest,
       MAYBE_CloseUIInPersistentModeDemotesWidget) {
  if (g_browser_process && g_browser_process->local_state()) {
    g_browser_process->local_state()->SetBoolean(
        omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel, false);
  }
  auto ui_manager = CreateUIManager();

  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* widget = ui_manager->widget();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());
  views::test::WaitForWidgetActive(widget, true);
  EXPECT_TRUE(ui_manager->IsActive());
  EXPECT_EQ(widget->GetZOrderLevel(), ui::ZOrderLevel::kNormal);

  // CloseUI() in persistent mode should demote (deactivate and keep visible).
  ui_manager->CloseUI();
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_FALSE(ui_manager->IsActive());
  EXPECT_EQ(widget->GetZOrderLevel(), ui::ZOrderLevel::kNormal);

  ui_manager->Close();
}

// Tests that CloseUI() closes and hides the widget in ephemeral mode.
TEST_F(OmniboxEverywhereUIManagerTest, CloseUIInEphemeralModeClosesWidget) {
  if (g_browser_process && g_browser_process->local_state()) {
    g_browser_process->local_state()->SetBoolean(
        omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel, true);
  }
  auto ui_manager = CreateUIManager();

  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* widget = ui_manager->widget();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());

  // CloseUI() in ephemeral mode should close/hide the widget.
  ui_manager->CloseUI();
  EXPECT_FALSE(ui_manager->IsVisible());
}

TEST_F(OmniboxEverywhereUIManagerTest, DismissBypassedDuringFileChooser) {
  if (g_browser_process && g_browser_process->local_state()) {
    g_browser_process->local_state()->SetBoolean(
        omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel, true);
  }
  auto ui_manager = CreateUIManager();

  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* widget = ui_manager->widget();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());

  // Mark file chooser as open.
  ui_manager->OnFileChooserOpened();
  EXPECT_TRUE(ui_manager->is_file_chooser_open_for_testing());

  // Simulating deactivation while a file chooser is open should NOT close the
  // widget.
  ui_manager->OnWidgetActivationChanged(widget, /*active=*/false);
  EXPECT_TRUE(ui_manager->widget());
  EXPECT_TRUE(widget->IsVisible());

  // Clean up: closing file chooser and triggering deactivation after grace
  // period should hide the widget in ephemeral mode.
  task_environment()->FastForwardBy(
      omnibox_everywhere::OmniboxEverywhereUIManager::kActivationGracePeriod +
      base::Milliseconds(1));
  ui_manager->OnFileChooserClosed();
  ui_manager->OnWidgetActivationChanged(widget, /*active=*/false);
  EXPECT_TRUE(base::test::RunUntil([&]() { return !widget->IsVisible(); }));
}

TEST_F(OmniboxEverywhereUIManagerTest, MultiProfileSwapping) {
  TestingProfile profile2;
  auto ui_manager = CreateUIManager();

  ui_manager->ShowForProfile(&profile_, GetContext());
  EXPECT_EQ(ui_manager->profile(), &profile_);
  views::Widget* widget = ui_manager->widget();
  ASSERT_TRUE(widget);

  // Swapping to profile2 should update the active profile on the same UIManager
  // shell.
  ui_manager->ShowForProfile(&profile2, GetContext());
  EXPECT_EQ(ui_manager->profile(), &profile2);
  EXPECT_TRUE(ui_manager->widget());

  // Clean up.
  ui_manager->Shutdown();
  EXPECT_FALSE(ui_manager->widget());
}

TEST_F(OmniboxEverywhereUIManagerTest,
       ShowForProfileReactivatesExistingWidget) {
  auto ui_manager = CreateUIManager();

  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* widget = ui_manager->widget();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());

  // ShowForProfile when already visible for the same profile should NOT close
  // the widget.
  ui_manager->ShowForProfile(&profile_, GetContext());
  EXPECT_EQ(ui_manager->widget(), widget);
  EXPECT_TRUE(widget->IsVisible());

  ui_manager->Close();
}

TEST_F(OmniboxEverywhereUIManagerTest, ShutdownSynchronouslyDestroysResources) {
  auto ui_manager = CreateUIManager();

  ui_manager->ShowForProfile(&profile_, GetContext());
  ASSERT_TRUE(ui_manager->widget());
  ASSERT_TRUE(ui_manager->contents_wrapper_for_testing());

  ui_manager->Shutdown();

  EXPECT_FALSE(ui_manager->widget());
  EXPECT_FALSE(ui_manager->contents_wrapper_for_testing());
  EXPECT_EQ(ui_manager->profile(), nullptr);
  EXPECT_FALSE(ui_manager->is_file_chooser_open_for_testing());
  EXPECT_FALSE(ui_manager->is_permission_prompt_open_for_testing());
}

#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_ShowPositionsOnTargetDisplay DISABLED_ShowPositionsOnTargetDisplay
#else
#define MAYBE_ShowPositionsOnTargetDisplay ShowPositionsOnTargetDisplay
#endif
TEST_F(OmniboxEverywhereUIManagerTest, MAYBE_ShowPositionsOnTargetDisplay) {
  // Create and set up a TestScreen with two displays.
  // Display 1: 0, 0, 800, 600 (Primary)
  // Display 2: 800, 0, 1024, 768 (Secondary)
  display::test::TestScreen test_screen(/*create_display=*/false,
                                        /*register_screen=*/false);
  ScopedScreenOverride screen_override(&test_screen);

  display::Display display1(1, gfx::Rect(0, 0, 800, 600));
  display::Display display2(2, gfx::Rect(800, 0, 1024, 768));
  test_screen.display_list().AddDisplay(display1,
                                        display::DisplayList::Type::PRIMARY);
  test_screen.display_list().AddDisplay(
      display2, display::DisplayList::Type::NOT_PRIMARY);

  // Set the fake cursor on the second display.
  test_screen.set_cursor_screen_point(gfx::Point(1200, 300));

  auto ui_manager = CreateUIManager();
  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* widget = ui_manager->widget();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());

  gfx::Rect widget_bounds = widget->GetWindowBoundsInScreen();

  // Verify that the widget was positioned on the secondary display (nearest to
  // cursor).
  display::Display current_display =
      display::Screen::Get()->GetDisplayMatching(widget_bounds);
  EXPECT_NE(current_display.id(),
            display::Screen::Get()->GetPrimaryDisplay().id());

  ui_manager->Close();
}

#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_PreservePositionAcrossDisplaysOnReinvoke \
  DISABLED_PreservePositionAcrossDisplaysOnReinvoke
#else
#define MAYBE_PreservePositionAcrossDisplaysOnReinvoke \
  PreservePositionAcrossDisplaysOnReinvoke
#endif
TEST_F(OmniboxEverywhereUIManagerTest,
       MAYBE_PreservePositionAcrossDisplaysOnReinvoke) {
  display::test::TestScreen test_screen(/*create_display=*/false,
                                        /*register_screen=*/false);
  ScopedScreenOverride screen_override(&test_screen);

  display::Display display1(1, gfx::Rect(0, 0, 800, 600));
  display::Display display2(2, gfx::Rect(800, 0, 1024, 768));
  test_screen.display_list().AddDisplay(display1,
                                        display::DisplayList::Type::PRIMARY);
  test_screen.display_list().AddDisplay(
      display2, display::DisplayList::Type::NOT_PRIMARY);

  // Set the fake cursor on the second display initially.
  test_screen.set_cursor_screen_point(gfx::Point(1200, 300));

  auto ui_manager = CreateUIManager();
  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* widget = ui_manager->widget();
  ASSERT_TRUE(widget);

  gfx::Rect initial_bounds = widget->GetWindowBoundsInScreen();

  // Move the cursor to the first display and re-invoke ShowForProfile.
  test_screen.set_cursor_screen_point(gfx::Point(100, 100));
  ui_manager->ShowForProfile(&profile_, GetContext());

  // The widget bounds should remain unchanged on the secondary display.
  EXPECT_EQ(widget->GetWindowBoundsInScreen(), initial_bounds);

  ui_manager->Close();
}

TEST_F(OmniboxEverywhereUIManagerTest, DrivePickerStateTracking) {
  auto ui_manager = CreateUIManager();
  ui_manager->ShowForProfile(&profile_, GetContext());
  ASSERT_TRUE(ui_manager->web_contents());

  EXPECT_FALSE(ui_manager->is_drive_picker_open_for_testing());
  EXPECT_FALSE(ui_manager->web_contents()->ShouldIgnoreInputEventsForTesting());

  ui_manager->OnDrivePickerOpened();
  EXPECT_TRUE(ui_manager->is_drive_picker_open_for_testing());
  EXPECT_TRUE(ui_manager->web_contents()->ShouldIgnoreInputEventsForTesting());

  ui_manager->OnDrivePickerClosed();
  EXPECT_FALSE(ui_manager->is_drive_picker_open_for_testing());
  EXPECT_FALSE(ui_manager->web_contents()->ShouldIgnoreInputEventsForTesting());
}

TEST_F(OmniboxEverywhereUIManagerTest, DismissBypassedDuringDrivePicker) {
  if (g_browser_process && g_browser_process->local_state()) {
    g_browser_process->local_state()->SetBoolean(
        omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel, true);
  }
  auto ui_manager = CreateUIManager();

  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* widget = ui_manager->widget();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());

  // Mark drive picker as open.
  ui_manager->OnDrivePickerOpened();
  EXPECT_TRUE(ui_manager->is_drive_picker_open_for_testing());

  // Simulating deactivation while drive picker is open should NOT close the
  // widget.
  ui_manager->OnWidgetActivationChanged(widget, /*active=*/false);
  EXPECT_TRUE(ui_manager->widget());
  EXPECT_TRUE(widget->IsVisible());

  // Clean up: closing drive picker and triggering deactivation after grace
  // period should hide the widget in ephemeral mode.
  task_environment()->FastForwardBy(
      omnibox_everywhere::OmniboxEverywhereUIManager::kActivationGracePeriod +
      base::Milliseconds(1));
  ui_manager->OnDrivePickerClosed();
  ui_manager->OnWidgetActivationChanged(widget, /*active=*/false);
  EXPECT_TRUE(base::test::RunUntil([&]() { return !widget->IsVisible(); }));
}

TEST_F(OmniboxEverywhereUIManagerTest, CloseDestroysWidgetWhenChooserOpen) {
  auto ui_manager = CreateUIManager();

  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* widget = ui_manager->widget();
  ASSERT_TRUE(widget);

  // Open file chooser.
  ui_manager->OnFileChooserOpened();

  // Close() while file chooser is open should destroy widget to prevent
  // orphaned modals.
  ui_manager->Close();
  EXPECT_FALSE(ui_manager->widget());
}

TEST_F(OmniboxEverywhereUIManagerTest,
       BrowserCollectionObserverNoCrashWhenNull) {
  auto ui_manager = CreateUIManager();
  MockBrowserWindowInterface mock_browser;
  ui_manager->OnBrowserActivated(&mock_browser);
  ui_manager->OnBrowserClosed(&mock_browser);
}

TEST_F(OmniboxEverywhereUIManagerTest,
       DraggableRegionsChangedAndEventHandling) {
  auto ui_manager = CreateUIManager();
  ui_manager->ShowForProfile(&profile_, GetContext());
  ASSERT_TRUE(ui_manager->widget_delegate());

  std::vector<blink::mojom::DraggableRegionPtr> regions;

  auto drag_region = blink::mojom::DraggableRegion::New();
  drag_region->bounds = gfx::Rect(0, 0, 800, 600);
  drag_region->draggable = true;
  regions.push_back(std::move(drag_region));

  auto no_drag_input_region = blink::mojom::DraggableRegion::New();
  no_drag_input_region->bounds = gfx::Rect(100, 30, 400, 50);
  no_drag_input_region->draggable = false;
  regions.push_back(std::move(no_drag_input_region));

  ui_manager->DraggableRegionsChanged(regions, nullptr);

  // Background point (draggable region).
  EXPECT_TRUE(ui_manager->widget_delegate()->IsPointInDraggableRegion(
      gfx::Point(10, 10)));
  EXPECT_TRUE(ui_manager->widget_delegate()->IsPointInDraggableRegion(
      gfx::Point(600, 40)));

  // Points inside search input (non-draggable region).
  EXPECT_FALSE(ui_manager->widget_delegate()->IsPointInDraggableRegion(
      gfx::Point(200, 40)));
  EXPECT_FALSE(ui_manager->widget_delegate()->IsPointInDraggableRegion(
      gfx::Point(400, 50)));

  // Event targeting should descend into child views for both draggable and
  // non-draggable regions so WebUI receives :hover and click events while
  // OmniboxEverywhereEventHandlerAura intercepts drags as a pre-target handler.
  EXPECT_TRUE(
      ui_manager->widget_delegate()->ShouldDescendIntoChildForEventHandling(
          gfx::NativeView(), gfx::Point(10, 10)));
  EXPECT_TRUE(
      ui_manager->widget_delegate()->ShouldDescendIntoChildForEventHandling(
          gfx::NativeView(), gfx::Point(200, 40)));
}

TEST_F(OmniboxEverywhereUIManagerTest,
       NonDraggableRegionTakesPrecedenceOverSubsequentDraggableRegion) {
  auto ui_manager = CreateUIManager();
  ui_manager->ShowForProfile(&profile_, GetContext());
  ASSERT_TRUE(ui_manager->widget_delegate());

  std::vector<blink::mojom::DraggableRegionPtr> regions;

  // Non-draggable region added before the overlapping draggable region.
  auto no_drag_region = blink::mojom::DraggableRegion::New();
  no_drag_region->bounds = gfx::Rect(100, 100, 200, 200);
  no_drag_region->draggable = false;
  regions.push_back(std::move(no_drag_region));

  // Large draggable region enclosing the non-draggable region added afterwards.
  auto drag_region = blink::mojom::DraggableRegion::New();
  drag_region->bounds = gfx::Rect(0, 0, 800, 600);
  drag_region->draggable = true;
  regions.push_back(std::move(drag_region));

  ui_manager->DraggableRegionsChanged(regions, nullptr);

  // Inner non-draggable area should remain non-draggable.
  EXPECT_FALSE(ui_manager->widget_delegate()->IsPointInDraggableRegion(
      gfx::Point(150, 150)));

  // Draggable background area outside should remain draggable.
  EXPECT_TRUE(ui_manager->widget_delegate()->IsPointInDraggableRegion(
      gfx::Point(10, 10)));
}

TEST_F(OmniboxEverywhereUIManagerTest, EarlyDraggableRegionsChangedPreserved) {
  auto ui_manager = CreateUIManager();
  std::vector<blink::mojom::DraggableRegionPtr> regions;

  auto drag_region = blink::mojom::DraggableRegion::New();
  drag_region->bounds = gfx::Rect(0, 0, 800, 600);
  drag_region->draggable = true;
  regions.push_back(std::move(drag_region));

  // Trigger region update BEFORE ShowUI / widget creation.
  ui_manager->DraggableRegionsChanged(regions, nullptr);

  // Now create widget via ShowForProfile.
  ui_manager->ShowForProfile(&profile_, GetContext());
  ASSERT_TRUE(ui_manager->widget_delegate());

  // Cached region should be applied to widget_delegate_.
  EXPECT_TRUE(ui_manager->widget_delegate()->IsPointInDraggableRegion(
      gfx::Point(10, 10)));
}

TEST_F(OmniboxEverywhereUIManagerTest, DismissBypassedDuringContextMenu) {
  if (g_browser_process && g_browser_process->local_state()) {
    g_browser_process->local_state()->SetBoolean(
        omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel, true);
  }
  auto ui_manager = CreateUIManager();

  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* widget = ui_manager->widget();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());

  // Mark context menu as open.
  ui_manager->set_is_context_menu_open_for_testing(true);
  EXPECT_TRUE(ui_manager->is_context_menu_open_for_testing());

  // Simulating deactivation while context menu is open should NOT close the
  // widget.
  ui_manager->OnWidgetActivationChanged(widget, /*active=*/false);
  EXPECT_TRUE(ui_manager->widget());
  EXPECT_TRUE(widget->IsVisible());

  // Clean up: closing context menu and triggering deactivation after grace
  // period should close the widget in ephemeral mode.
  task_environment()->FastForwardBy(
      omnibox_everywhere::OmniboxEverywhereUIManager::kActivationGracePeriod +
      base::Milliseconds(1));
  ui_manager->OnContextMenuClosedForTesting();
  ui_manager->OnWidgetActivationChanged(widget, /*active=*/false);
  EXPECT_TRUE(base::test::RunUntil([&]() { return !widget->IsVisible(); }));
  EXPECT_TRUE(ui_manager->widget());
}

TEST_F(OmniboxEverywhereUIManagerTest, CloseCancelsOpenContextMenu) {
  auto ui_manager = CreateUIManager();

  ui_manager->ShowForProfile(&profile_, GetContext());
  ASSERT_TRUE(ui_manager->widget());

  auto* rfh = ui_manager->contents_wrapper_for_testing()
                  ->web_contents()
                  ->GetPrimaryMainFrame();
  content::ContextMenuParams params;
  params.is_editable = true;

  bool menu_runner_created = false;
  ui_manager->SetMenuRunnerFactoryForTesting(base::BindRepeating(
      [](bool* created, ui::MenuModel* model,
         base::RepeatingClosure on_closed) {
        *created = true;
        auto runner = std::make_unique<views::MenuRunner>(
            model,
            views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU,
            on_closed);
        views::test::MenuRunnerTestAPI(runner.get())
            .SetMenuRunnerHandler(std::make_unique<TestMenuRunnerHandler>());
        return runner;
      },
      &menu_runner_created));

  ui_manager->HandleContextMenu(*rfh, params);
  EXPECT_TRUE(menu_runner_created);
  EXPECT_TRUE(ui_manager->is_context_menu_open_for_testing());

  // Calling Close() while context menu is open should cancel the runner and
  // reset state.
  ui_manager->Close();
  EXPECT_FALSE(ui_manager->is_context_menu_open_for_testing());
  EXPECT_FALSE(ui_manager->widget()->IsVisible());
}

TEST_F(OmniboxEverywhereUIManagerTest, ContextMenuModelEditableElement) {
  auto ui_manager = CreateUIManager();
  ui_manager->ShowForProfile(&profile_, GetContext());
  ASSERT_TRUE(ui_manager->widget());

  auto* rfh = ui_manager->contents_wrapper_for_testing()
                  ->web_contents()
                  ->GetPrimaryMainFrame();

  content::ContextMenuParams params;
  params.is_editable = true;
  EXPECT_TRUE(ui_manager->HandleContextMenu(*rfh, params));

  const ui::SimpleMenuModel* model =
      ui_manager->context_menu_model_for_testing();
  ASSERT_TRUE(model);

  size_t index = 0;
  EXPECT_EQ(model->GetCommandIdAt(index++),
            omnibox_everywhere::OmniboxEverywhereUIManager::kUndo);
  EXPECT_EQ(model->GetTypeAt(index++), ui::MenuModel::ItemType::TYPE_SEPARATOR);
  EXPECT_EQ(model->GetCommandIdAt(index++),
            omnibox_everywhere::OmniboxEverywhereUIManager::kCut);
  EXPECT_EQ(model->GetCommandIdAt(index++),
            omnibox_everywhere::OmniboxEverywhereUIManager::kCopy);
  EXPECT_EQ(model->GetCommandIdAt(index++),
            omnibox_everywhere::OmniboxEverywhereUIManager::kPaste);
  EXPECT_EQ(model->GetCommandIdAt(index++),
            omnibox_everywhere::OmniboxEverywhereUIManager::kPasteAndSearch);
  EXPECT_EQ(model->GetCommandIdAt(index++),
            omnibox_everywhere::OmniboxEverywhereUIManager::kDelete);
  EXPECT_EQ(model->GetTypeAt(index++), ui::MenuModel::ItemType::TYPE_SEPARATOR);
  EXPECT_EQ(model->GetCommandIdAt(index++),
            omnibox_everywhere::OmniboxEverywhereUIManager::kSelectAll);
  EXPECT_EQ(model->GetTypeAt(index++), ui::MenuModel::ItemType::TYPE_SEPARATOR);
  EXPECT_EQ(
      model->GetCommandIdAt(index++),
      omnibox_everywhere::OmniboxEverywhereUIManager::kManageSearchEngines);
  EXPECT_EQ(model->GetTypeAt(index++), ui::MenuModel::ItemType::TYPE_SEPARATOR);
  EXPECT_EQ(model->GetCommandIdAt(index++),
            omnibox_everywhere::OmniboxEverywhereUIManager::kShowShortcuts);
  EXPECT_EQ(model->GetCommandIdAt(index++),
            omnibox_everywhere::OmniboxEverywhereUIManager::
                kCustomizeKeyboardShortcut);
  EXPECT_EQ(model->GetCommandIdAt(index++),
            omnibox_everywhere::OmniboxEverywhereUIManager::kSettings);
#if BUILDFLAG(IS_WIN)
  EXPECT_EQ(model->GetTypeAt(index++), ui::MenuModel::ItemType::TYPE_SEPARATOR);
  EXPECT_EQ(model->GetCommandIdAt(index++),
            omnibox_everywhere::OmniboxEverywhereUIManager::kMinimize);
  EXPECT_EQ(model->GetCommandIdAt(index++),
            omnibox_everywhere::OmniboxEverywhereUIManager::kClose);
#endif  // BUILDFLAG(IS_WIN)
  EXPECT_EQ(model->GetItemCount(), index);
}

TEST_F(OmniboxEverywhereUIManagerTest,
       ContextMenuModelNonEditableElementWithSelection) {
  auto ui_manager = CreateUIManager();
  ui_manager->ShowForProfile(&profile_, GetContext());
  ASSERT_TRUE(ui_manager->widget());

  auto* rfh = ui_manager->contents_wrapper_for_testing()
                  ->web_contents()
                  ->GetPrimaryMainFrame();

  content::ContextMenuParams params;
  params.is_editable = false;
  params.selection_text = u"selected text";
  EXPECT_TRUE(ui_manager->HandleContextMenu(*rfh, params));

  const ui::SimpleMenuModel* model =
      ui_manager->context_menu_model_for_testing();
  ASSERT_TRUE(model);
#if BUILDFLAG(IS_WIN)
  EXPECT_EQ(model->GetItemCount(), 12u);
#else
  EXPECT_EQ(model->GetItemCount(), 9u);
#endif  // BUILDFLAG(IS_WIN)
  EXPECT_EQ(model->GetCommandIdAt(0),
            omnibox_everywhere::OmniboxEverywhereUIManager::kCopy);
  EXPECT_EQ(model->GetTypeAt(1), ui::MenuModel::ItemType::TYPE_SEPARATOR);
  EXPECT_EQ(model->GetCommandIdAt(2),
            omnibox_everywhere::OmniboxEverywhereUIManager::kSelectAll);
  EXPECT_EQ(model->GetTypeAt(3), ui::MenuModel::ItemType::TYPE_SEPARATOR);
  EXPECT_EQ(
      model->GetCommandIdAt(4),
      omnibox_everywhere::OmniboxEverywhereUIManager::kManageSearchEngines);
  EXPECT_EQ(model->GetTypeAt(5), ui::MenuModel::ItemType::TYPE_SEPARATOR);
  EXPECT_EQ(model->GetCommandIdAt(6),
            omnibox_everywhere::OmniboxEverywhereUIManager::kShowShortcuts);
  EXPECT_EQ(model->GetCommandIdAt(7),
            omnibox_everywhere::OmniboxEverywhereUIManager::
                kCustomizeKeyboardShortcut);
  EXPECT_EQ(model->GetCommandIdAt(8),
            omnibox_everywhere::OmniboxEverywhereUIManager::kSettings);
#if BUILDFLAG(IS_WIN)
  EXPECT_EQ(model->GetTypeAt(9), ui::MenuModel::ItemType::TYPE_SEPARATOR);
  EXPECT_EQ(model->GetCommandIdAt(10),
            omnibox_everywhere::OmniboxEverywhereUIManager::kMinimize);
  EXPECT_EQ(model->GetCommandIdAt(11),
            omnibox_everywhere::OmniboxEverywhereUIManager::kClose);
#endif  // BUILDFLAG(IS_WIN)
}

TEST_F(OmniboxEverywhereUIManagerTest, ContextMenuModelNonEditableBackground) {
  auto ui_manager = CreateUIManager();
  ui_manager->ShowForProfile(&profile_, GetContext());
  ASSERT_TRUE(ui_manager->widget());

  auto* rfh = ui_manager->contents_wrapper_for_testing()
                  ->web_contents()
                  ->GetPrimaryMainFrame();

  // Right-clicking on empty background space (not editable, no selection text).
  content::ContextMenuParams params;
  params.is_editable = false;
  params.selection_text = u"";
  EXPECT_TRUE(ui_manager->HandleContextMenu(*rfh, params));

  const ui::SimpleMenuModel* model =
      ui_manager->context_menu_model_for_testing();
  ASSERT_TRUE(model);
#if BUILDFLAG(IS_WIN)
  EXPECT_EQ(model->GetItemCount(), 8u);
#else
  EXPECT_EQ(model->GetItemCount(), 5u);
#endif  // BUILDFLAG(IS_WIN)
  EXPECT_EQ(
      model->GetCommandIdAt(0),
      omnibox_everywhere::OmniboxEverywhereUIManager::kManageSearchEngines);
  EXPECT_EQ(model->GetTypeAt(1), ui::MenuModel::ItemType::TYPE_SEPARATOR);
  EXPECT_EQ(model->GetCommandIdAt(2),
            omnibox_everywhere::OmniboxEverywhereUIManager::kShowShortcuts);
  EXPECT_EQ(model->GetCommandIdAt(3),
            omnibox_everywhere::OmniboxEverywhereUIManager::
                kCustomizeKeyboardShortcut);
  EXPECT_EQ(model->GetCommandIdAt(4),
            omnibox_everywhere::OmniboxEverywhereUIManager::kSettings);
#if BUILDFLAG(IS_WIN)
  EXPECT_EQ(model->GetTypeAt(5), ui::MenuModel::ItemType::TYPE_SEPARATOR);
  EXPECT_EQ(model->GetCommandIdAt(6),
            omnibox_everywhere::OmniboxEverywhereUIManager::kMinimize);
  EXPECT_EQ(model->GetCommandIdAt(7),
            omnibox_everywhere::OmniboxEverywhereUIManager::kClose);
#endif  // BUILDFLAG(IS_WIN)
}

TEST_F(OmniboxEverywhereUIManagerTest,
       ShowContextMenuForViewOpensBackgroundContextMenu) {
  auto ui_manager = CreateUIManager();
  ui_manager->ShowForProfile(&profile_, GetContext());
  ASSERT_TRUE(ui_manager->widget());

  views::View* contents_view = ui_manager->widget()->GetContentsView();
  ASSERT_TRUE(contents_view);

  // Trigger context menu via views::ContextMenuController (e.g. right-click on
  // draggable background region).
  ui_manager->ShowContextMenuForView(contents_view, gfx::Point(50, 50),
                                     ui::mojom::MenuSourceType::kMouse);

  const ui::SimpleMenuModel* model =
      ui_manager->context_menu_model_for_testing();
  ASSERT_TRUE(model);
#if BUILDFLAG(IS_WIN)
  EXPECT_EQ(model->GetItemCount(), 8u);
#else
  EXPECT_EQ(model->GetItemCount(), 5u);
#endif  // BUILDFLAG(IS_WIN)
  EXPECT_EQ(
      model->GetCommandIdAt(0),
      omnibox_everywhere::OmniboxEverywhereUIManager::kManageSearchEngines);
  EXPECT_EQ(model->GetTypeAt(1), ui::MenuModel::ItemType::TYPE_SEPARATOR);
  EXPECT_EQ(model->GetCommandIdAt(2),
            omnibox_everywhere::OmniboxEverywhereUIManager::kShowShortcuts);
  EXPECT_EQ(model->GetCommandIdAt(3),
            omnibox_everywhere::OmniboxEverywhereUIManager::
                kCustomizeKeyboardShortcut);
  EXPECT_EQ(model->GetCommandIdAt(4),
            omnibox_everywhere::OmniboxEverywhereUIManager::kSettings);
#if BUILDFLAG(IS_WIN)
  EXPECT_EQ(model->GetTypeAt(5), ui::MenuModel::ItemType::TYPE_SEPARATOR);
  EXPECT_EQ(model->GetCommandIdAt(6),
            omnibox_everywhere::OmniboxEverywhereUIManager::kMinimize);
  EXPECT_EQ(model->GetCommandIdAt(7),
            omnibox_everywhere::OmniboxEverywhereUIManager::kClose);
#endif  // BUILDFLAG(IS_WIN)
}

#if BUILDFLAG(IS_WIN)
TEST_F(OmniboxEverywhereUIManagerTest,
       ContextMenuOmitsWindowControlsWhenEphemeral) {
  base::ScopedClosureRunner reset_pref;
  if (g_browser_process && g_browser_process->local_state()) {
    g_browser_process->local_state()->SetBoolean(
        omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel, true);
    reset_pref.ReplaceClosure(base::BindOnce([]() {
      if (g_browser_process && g_browser_process->local_state()) {
        g_browser_process->local_state()->ClearPref(
            omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel);
      }
    }));
  }
  auto ui_manager = CreateUIManager();
  ui_manager->ShowForProfile(&profile_, GetContext());
  ASSERT_TRUE(ui_manager->widget());

  auto* rfh = ui_manager->contents_wrapper_for_testing()
                  ->web_contents()
                  ->GetPrimaryMainFrame();

  content::ContextMenuParams params;
  params.is_editable = false;
  EXPECT_TRUE(ui_manager->HandleContextMenu(*rfh, params));

  const ui::SimpleMenuModel* model =
      ui_manager->context_menu_model_for_testing();
  ASSERT_TRUE(model);
  EXPECT_EQ(model->GetItemCount(), 5u);
  EXPECT_EQ(model->GetCommandIdAt(4),
            omnibox_everywhere::OmniboxEverywhereUIManager::kSettings);
}

TEST_F(OmniboxEverywhereUIManagerTest, ExecuteCloseCommandHidesWidget) {
  auto ui_manager = CreateUIManager();
  ui_manager->ShowForProfile(&profile_, GetContext());
  ASSERT_TRUE(ui_manager->widget());
  ASSERT_TRUE(ui_manager->IsVisible());

  ui_manager->ExecuteCommand(
      omnibox_everywhere::OmniboxEverywhereUIManager::kClose, 0);

  // Close hides the widget but keeps it reshowable via hotkey or tray icon.
  EXPECT_TRUE(ui_manager->widget());
  EXPECT_FALSE(ui_manager->IsVisible());
}

TEST_F(OmniboxEverywhereUIManagerTest, ExecuteMinimizeCommandKeepsWidgetAlive) {
  auto ui_manager = CreateUIManager();
  ui_manager->ShowForProfile(&profile_, GetContext());
  ASSERT_TRUE(ui_manager->widget());
  ASSERT_TRUE(ui_manager->IsVisible());

  ui_manager->ExecuteCommand(
      omnibox_everywhere::OmniboxEverywhereUIManager::kMinimize, 0);

  EXPECT_TRUE(ui_manager->widget());
  EXPECT_TRUE(ui_manager->widget()->IsMinimized());
  EXPECT_TRUE(ui_manager->web_contents());
}

TEST_F(OmniboxEverywhereUIManagerTest, MinimizeDoesNotRecordFreImpression) {
  auto ui_manager = CreateUIManager();
  ui_manager->ShowForProfile(&profile_, GetContext());
  ASSERT_TRUE(ui_manager->widget());
  ASSERT_TRUE(ui_manager->IsVisible());

  // An impression bumps the counter for whichever FRE stage is active.
  PrefService* prefs = profile_.GetPrefs();
  auto fre_impression_counts = [prefs]() {
    return std::make_tuple(
        prefs->GetInteger(omnibox_everywhere::prefs::kFreIntroImpressionCount),
        prefs->GetInteger(
            omnibox_everywhere::prefs::kFreShortcutSetupImpressionCount),
        prefs->GetInteger(
            omnibox_everywhere::prefs::kFreShortcutReminderImpressionCount));
  };
  const auto counts_before = fre_impression_counts();

  ui_manager->ExecuteCommand(
      omnibox_everywhere::OmniboxEverywhereUIManager::kMinimize, 0);
  EXPECT_EQ(counts_before, fre_impression_counts());

  // Close does record one, so the expectation above isn't vacuous.
  ui_manager->ExecuteCommand(
      omnibox_everywhere::OmniboxEverywhereUIManager::kClose, 0);
  EXPECT_NE(counts_before, fre_impression_counts());
}
#endif  // BUILDFLAG(IS_WIN)

TEST_F(OmniboxEverywhereUIManagerTest, ContextMenuCommandEnablement) {
  SetUpAimEligibilityService(/*is_fusebox_eligible=*/true);
  SetUpAiModeButtonService();

  auto ui_manager = CreateUIManager();
  ui_manager->ShowForProfile(&profile_, GetContext());
  ASSERT_TRUE(ui_manager->widget());

  auto* rfh = ui_manager->contents_wrapper_for_testing()
                  ->web_contents()
                  ->GetPrimaryMainFrame();

  content::ContextMenuParams params;
  params.is_editable = true;
  params.edit_flags = blink::ContextMenuDataEditFlags::kCanCut |
                      blink::ContextMenuDataEditFlags::kCanCopy |
                      blink::ContextMenuDataEditFlags::kCanUndo |
                      blink::ContextMenuDataEditFlags::kCanDelete;
  ui_manager->HandleContextMenu(*rfh, params);

  EXPECT_TRUE(ui_manager->IsCommandIdEnabled(
      omnibox_everywhere::OmniboxEverywhereUIManager::kUndo));
  EXPECT_TRUE(ui_manager->IsCommandIdEnabled(
      omnibox_everywhere::OmniboxEverywhereUIManager::kCut));
  EXPECT_TRUE(ui_manager->IsCommandIdEnabled(
      omnibox_everywhere::OmniboxEverywhereUIManager::kCopy));
  EXPECT_TRUE(ui_manager->IsCommandIdEnabled(
      omnibox_everywhere::OmniboxEverywhereUIManager::kPaste));
  EXPECT_TRUE(ui_manager->IsCommandIdEnabled(
      omnibox_everywhere::OmniboxEverywhereUIManager::kDelete));
  EXPECT_TRUE(ui_manager->IsCommandIdEnabled(
      omnibox_everywhere::OmniboxEverywhereUIManager::kSelectAll));
  EXPECT_TRUE(ui_manager->IsCommandIdEnabled(
      omnibox_everywhere::OmniboxEverywhereUIManager::kManageSearchEngines));
  EXPECT_TRUE(ui_manager->IsCommandIdEnabled(
      omnibox_everywhere::OmniboxEverywhereUIManager::kAlwaysShowAiMode));
  EXPECT_TRUE(ui_manager->IsCommandIdEnabled(
      omnibox_everywhere::OmniboxEverywhereUIManager::kShowShortcuts));
  EXPECT_TRUE(ui_manager->IsCommandIdEnabled(
      omnibox_everywhere::OmniboxEverywhereUIManager::
          kCustomizeKeyboardShortcut));
  EXPECT_TRUE(ui_manager->IsCommandIdEnabled(
      omnibox_everywhere::OmniboxEverywhereUIManager::kSettings));
#if BUILDFLAG(IS_WIN)
  EXPECT_TRUE(ui_manager->IsCommandIdEnabled(
      omnibox_everywhere::OmniboxEverywhereUIManager::kMinimize));
  EXPECT_TRUE(ui_manager->IsCommandIdEnabled(
      omnibox_everywhere::OmniboxEverywhereUIManager::kClose));
#endif  // BUILDFLAG(IS_WIN)

  EXPECT_TRUE(ui_manager->IsCommandIdEnabled(
      omnibox_everywhere::OmniboxEverywhereUIManager::kPaste));
  EXPECT_TRUE(ui_manager->IsCommandIdEnabled(
      omnibox_everywhere::OmniboxEverywhereUIManager::kPasteAndSearch));

  // Without edit flags, Cut, Copy, Undo, and Delete should be disabled if
  // selection is empty.
  params.edit_flags = 0;
  params.selection_text = u"";
  ui_manager->HandleContextMenu(*rfh, params);

  EXPECT_FALSE(ui_manager->IsCommandIdEnabled(
      omnibox_everywhere::OmniboxEverywhereUIManager::kUndo));
  EXPECT_FALSE(ui_manager->IsCommandIdEnabled(
      omnibox_everywhere::OmniboxEverywhereUIManager::kCut));
  EXPECT_FALSE(ui_manager->IsCommandIdEnabled(
      omnibox_everywhere::OmniboxEverywhereUIManager::kCopy));
  EXPECT_FALSE(ui_manager->IsCommandIdEnabled(
      omnibox_everywhere::OmniboxEverywhereUIManager::kDelete));
  EXPECT_TRUE(ui_manager->IsCommandIdEnabled(
      omnibox_everywhere::OmniboxEverywhereUIManager::kPaste));
  EXPECT_TRUE(ui_manager->IsCommandIdEnabled(
      omnibox_everywhere::OmniboxEverywhereUIManager::kPasteAndSearch));
  EXPECT_TRUE(ui_manager->IsCommandIdEnabled(
      omnibox_everywhere::OmniboxEverywhereUIManager::kSelectAll));

  // When not editable and without kCanPaste flag, Paste is disabled.
  params.is_editable = false;
  ui_manager->HandleContextMenu(*rfh, params);
  EXPECT_FALSE(ui_manager->IsCommandIdEnabled(
      omnibox_everywhere::OmniboxEverywhereUIManager::kPaste));

  // When not editable but kCanPaste flag is explicitly present, Paste is
  // enabled.
  params.edit_flags = blink::ContextMenuDataEditFlags::kCanPaste;
  ui_manager->HandleContextMenu(*rfh, params);
  EXPECT_TRUE(ui_manager->IsCommandIdEnabled(
      omnibox_everywhere::OmniboxEverywhereUIManager::kPaste));
}

TEST_F(OmniboxEverywhereUIManagerTest, ExecutePasteAndSearchCommand) {
  auto ui_manager = CreateUIManager();
  ui_manager->ShowForProfile(&profile_, GetContext());
  ASSERT_TRUE(ui_manager->widget());

  {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
    writer.WriteText(u"https://example.com/");
  }

  EXPECT_TRUE(ui_manager->IsCommandIdEnabled(
      omnibox_everywhere::OmniboxEverywhereUIManager::kPasteAndSearch));
  ui_manager->ExecuteCommand(
      omnibox_everywhere::OmniboxEverywhereUIManager::kPasteAndSearch, 0);
}

TEST_F(OmniboxEverywhereUIManagerTest,
       ContextMenuManageSearchEnginesSafetyGuard) {
  auto ui_manager = CreateUIManager();
  // Before showing (no WebContents / no profile), commands are disabled.
  EXPECT_FALSE(ui_manager->IsCommandIdEnabled(
      omnibox_everywhere::OmniboxEverywhereUIManager::kManageSearchEngines));

  ui_manager->ShowForProfile(&profile_, GetContext());
  EXPECT_TRUE(ui_manager->IsCommandIdEnabled(
      omnibox_everywhere::OmniboxEverywhereUIManager::kManageSearchEngines));

  ui_manager->Shutdown();
  EXPECT_FALSE(ui_manager->IsCommandIdEnabled(
      omnibox_everywhere::OmniboxEverywhereUIManager::kManageSearchEngines));
}

TEST_F(OmniboxEverywhereUIManagerTest, ContextMenuAiModeFuseboxIneligible) {
  SetUpAimEligibilityService(/*is_fusebox_eligible=*/false);
  SetUpAiModeButtonService();

  auto ui_manager = CreateUIManager();
  ui_manager->ShowForProfile(&profile_, GetContext());
  ASSERT_TRUE(ui_manager->widget());

  EXPECT_FALSE(ui_manager->IsCommandIdEnabled(
      omnibox_everywhere::OmniboxEverywhereUIManager::kAlwaysShowAiMode));

  auto* rfh = ui_manager->contents_wrapper_for_testing()
                  ->web_contents()
                  ->GetPrimaryMainFrame();
  content::ContextMenuParams params;
  params.is_editable = false;
  params.selection_text = u"";
  EXPECT_TRUE(ui_manager->HandleContextMenu(*rfh, params));

  const ui::SimpleMenuModel* model =
      ui_manager->context_menu_model_for_testing();
  ASSERT_TRUE(model);
  EXPECT_FALSE(
      model
          ->GetIndexOfCommandId(
              omnibox_everywhere::OmniboxEverywhereUIManager::kAlwaysShowAiMode)
          .has_value());
}

TEST_F(OmniboxEverywhereUIManagerTest, ContextMenuAiModeFuseboxEligible) {
  SetUpAimEligibilityService(/*is_fusebox_eligible=*/true);
  SetUpAiModeButtonService();

  auto ui_manager = CreateUIManager();
  ui_manager->ShowForProfile(&profile_, GetContext());
  ASSERT_TRUE(ui_manager->widget());

  EXPECT_TRUE(ui_manager->IsCommandIdEnabled(
      omnibox_everywhere::OmniboxEverywhereUIManager::kAlwaysShowAiMode));

  auto* rfh = ui_manager->contents_wrapper_for_testing()
                  ->web_contents()
                  ->GetPrimaryMainFrame();
  content::ContextMenuParams params;
  params.is_editable = false;
  params.selection_text = u"";
  EXPECT_TRUE(ui_manager->HandleContextMenu(*rfh, params));

  const ui::SimpleMenuModel* model =
      ui_manager->context_menu_model_for_testing();
  ASSERT_TRUE(model);
  EXPECT_TRUE(
      model
          ->GetIndexOfCommandId(
              omnibox_everywhere::OmniboxEverywhereUIManager::kAlwaysShowAiMode)
          .has_value());
}

TEST_F(OmniboxEverywhereUIManagerTest, ContextMenuAlwaysShowAiModeToggle) {
  SetUpAimEligibilityService(/*is_fusebox_eligible=*/true);
  SetUpAiModeButtonService();

  auto ui_manager = CreateUIManager();
  ui_manager->ShowForProfile(&profile_, GetContext());
  ASSERT_TRUE(ui_manager->widget());

  profile_.GetPrefs()->SetBoolean(
      omnibox_everywhere::prefs::kOmniboxEverywhereShowAiMode, true);
  profile_.GetPrefs()->SetBoolean(omnibox::kShowAiModeOmniboxButton, true);
  EXPECT_TRUE(ui_manager->IsCommandIdChecked(
      omnibox_everywhere::OmniboxEverywhereUIManager::kAlwaysShowAiMode));

  ui_manager->ExecuteCommand(
      omnibox_everywhere::OmniboxEverywhereUIManager::kAlwaysShowAiMode, 0);
  EXPECT_FALSE(profile_.GetPrefs()->GetBoolean(
      omnibox_everywhere::prefs::kOmniboxEverywhereShowAiMode));
  EXPECT_FALSE(ui_manager->IsCommandIdChecked(
      omnibox_everywhere::OmniboxEverywhereUIManager::kAlwaysShowAiMode));
  // Verify that the browser omnibox preference remains unaffected.
  EXPECT_TRUE(
      profile_.GetPrefs()->GetBoolean(omnibox::kShowAiModeOmniboxButton));

  ui_manager->ExecuteCommand(
      omnibox_everywhere::OmniboxEverywhereUIManager::kAlwaysShowAiMode, 0);
  EXPECT_TRUE(profile_.GetPrefs()->GetBoolean(
      omnibox_everywhere::prefs::kOmniboxEverywhereShowAiMode));
  EXPECT_TRUE(ui_manager->IsCommandIdChecked(
      omnibox_everywhere::OmniboxEverywhereUIManager::kAlwaysShowAiMode));
  EXPECT_TRUE(
      profile_.GetPrefs()->GetBoolean(omnibox::kShowAiModeOmniboxButton));
}

TEST_F(OmniboxEverywhereUIManagerTest, ContextMenuShowShortcutsToggle) {
  auto ui_manager = CreateUIManager();
  ui_manager->ShowForProfile(&profile_, GetContext());
  ASSERT_TRUE(ui_manager->widget());

  profile_.GetPrefs()->SetInteger(
      omnibox_everywhere::prefs::kOmniboxEverywhereShowShortcuts,
      static_cast<int>(
          omnibox_everywhere::prefs::ShowShortcutsPrefValue::kEnabled));
  EXPECT_TRUE(ui_manager->IsCommandIdChecked(
      omnibox_everywhere::OmniboxEverywhereUIManager::kShowShortcuts));

  ui_manager->ExecuteCommand(
      omnibox_everywhere::OmniboxEverywhereUIManager::kShowShortcuts, 0);
  EXPECT_EQ(profile_.GetPrefs()->GetInteger(
                omnibox_everywhere::prefs::kOmniboxEverywhereShowShortcuts),
            static_cast<int>(
                omnibox_everywhere::prefs::ShowShortcutsPrefValue::kDisabled));
  EXPECT_FALSE(ui_manager->IsCommandIdChecked(
      omnibox_everywhere::OmniboxEverywhereUIManager::kShowShortcuts));

  ui_manager->ExecuteCommand(
      omnibox_everywhere::OmniboxEverywhereUIManager::kShowShortcuts, 0);
  EXPECT_EQ(profile_.GetPrefs()->GetInteger(
                omnibox_everywhere::prefs::kOmniboxEverywhereShowShortcuts),
            static_cast<int>(
                omnibox_everywhere::prefs::ShowShortcutsPrefValue::kEnabled));
  EXPECT_TRUE(ui_manager->IsCommandIdChecked(
      omnibox_everywhere::OmniboxEverywhereUIManager::kShowShortcuts));
}

TEST_F(OmniboxEverywhereUIManagerTest,
       ContextMenuShowShortcutsHiddenWhenNoShortcutsAvailable) {
  auto ui_manager = CreateUIManager();
  ui_manager->ShowForProfile(&profile_, GetContext());
  ASSERT_TRUE(ui_manager->widget());

  auto* rfh = ui_manager->contents_wrapper_for_testing()
                  ->web_contents()
                  ->GetPrimaryMainFrame();
  content::ContextMenuParams params;
  params.is_editable = false;

  // By default, personal shortcuts are enabled, so the option should be
  // present.
  EXPECT_TRUE(ui_manager->HandleContextMenu(*rfh, params));
  const ui::SimpleMenuModel* model =
      ui_manager->context_menu_model_for_testing();
  ASSERT_TRUE(model);
  EXPECT_TRUE(
      model
          ->GetIndexOfCommandId(
              omnibox_everywhere::OmniboxEverywhereUIManager::kShowShortcuts)
          .has_value());

  // Without enterprise policy shortcuts, the option is always present.
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpPersonalShortcutsVisible,
                                  false);
  EXPECT_TRUE(ui_manager->HandleContextMenu(*rfh, params));
  model = ui_manager->context_menu_model_for_testing();
  ASSERT_TRUE(model);
  EXPECT_TRUE(
      model
          ->GetIndexOfCommandId(
              omnibox_everywhere::OmniboxEverywhereUIManager::kShowShortcuts)
          .has_value());

  // Add enterprise shortcuts policy so both checkboxes exist.
  {
    ScopedListPrefUpdate update(
        profile_.GetPrefs(), ntp_tiles::prefs::kEnterpriseShortcutsPolicyList);
    update->Append("https://corp.example.com");
  }

  // When both checkboxes are unchecked, no shortcuts can be shown at all.
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpEnterpriseShortcutsVisible,
                                  false);
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpPersonalShortcutsVisible,
                                  false);

  EXPECT_TRUE(ui_manager->HandleContextMenu(*rfh, params));
  model = ui_manager->context_menu_model_for_testing();
  ASSERT_TRUE(model);
  EXPECT_FALSE(
      model
          ->GetIndexOfCommandId(
              omnibox_everywhere::OmniboxEverywhereUIManager::kShowShortcuts)
          .has_value());
}

// TODO(crbug.com/546710681): Re-enable test on linux
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
#define MAYBE_ResizeDueToAutoResizeUpdatesWidgetBounds \
  DISABLED_ResizeDueToAutoResizeUpdatesWidgetBounds
#else
#define MAYBE_ResizeDueToAutoResizeUpdatesWidgetBounds \
  ResizeDueToAutoResizeUpdatesWidgetBounds
#endif
TEST_F(OmniboxEverywhereUIManagerTest,
       MAYBE_ResizeDueToAutoResizeUpdatesWidgetBounds) {
  display::test::TestScreen test_screen(/*create_display=*/false,
                                        /*register_screen=*/false);
  display::Screen* old_screen = display::Screen::SetScreenInstance(nullptr);
  display::Screen::SetScreenInstance(&test_screen);
  base::ScopedClosureRunner screen_restorer(base::BindOnce(
      [](display::Screen* old_screen) {
        display::Screen::SetScreenInstance(nullptr);
        display::Screen::SetScreenInstance(old_screen);
      },
      old_screen));
  test_screen.display_list().AddDisplay(
      display::Display(1, gfx::Rect(0, 0, 1920, 1080)),
      display::DisplayList::Type::PRIMARY);

  auto ui_manager = CreateUIManager();
  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* widget = ui_manager->widget();
  ASSERT_TRUE(widget);

  const int popup_width =
      omnibox_everywhere::OmniboxEverywhereUIManager::GetPopupFixedWidth();

  EXPECT_EQ(widget->GetWindowBoundsInScreen().width(), popup_width);

  // Resize above minimum height should resize the widget height directly.
  ui_manager->ResizeDueToAutoResize(nullptr, gfx::Size(popup_width, 150));
  EXPECT_EQ(widget->GetWindowBoundsInScreen().height(), 150);
  EXPECT_EQ(widget->GetWindowBoundsInScreen().width(), popup_width);

  // Resize below minimum height (56) should clamp to 56.
  ui_manager->ResizeDueToAutoResize(nullptr, gfx::Size(popup_width, 30));
  EXPECT_EQ(widget->GetWindowBoundsInScreen().height(), 56);
  EXPECT_EQ(widget->GetWindowBoundsInScreen().width(), popup_width);

  // Even if widget width was temporarily modified (e.g. edge clamping),
  // ResizeDueToAutoResize enforces the fixed width.
  gfx::Rect clamped_bounds = widget->GetWindowBoundsInScreen();
  clamped_bounds.set_width(400);
  widget->SetBounds(clamped_bounds);
  EXPECT_EQ(widget->GetWindowBoundsInScreen().width(), 400);

  ui_manager->ResizeDueToAutoResize(nullptr, gfx::Size(popup_width, 200));
  EXPECT_EQ(widget->GetWindowBoundsInScreen().height(), 200);
  EXPECT_EQ(widget->GetWindowBoundsInScreen().width(), popup_width);

  // While dragging, AutoResize should be deferred.
  ui_manager->OnWidgetUserDragStarted(widget);
  ui_manager->ResizeDueToAutoResize(nullptr, gfx::Size(popup_width, 300));
  // Size remains unchanged during drag.
  EXPECT_EQ(widget->GetWindowBoundsInScreen().height(), 200);

  // When drag ends, the pending AutoResize is applied.
  ui_manager->OnWidgetUserDragEnded(widget);
  EXPECT_EQ(widget->GetWindowBoundsInScreen().height(), 300);

  ui_manager->Close();
}

#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_OnWidgetUserDragEndedAdjustsToFitWorkArea \
  DISABLED_OnWidgetUserDragEndedAdjustsToFitWorkArea
#else
#define MAYBE_OnWidgetUserDragEndedAdjustsToFitWorkArea \
  OnWidgetUserDragEndedAdjustsToFitWorkArea
#endif
TEST_F(OmniboxEverywhereUIManagerTest,
       MAYBE_OnWidgetUserDragEndedAdjustsToFitWorkArea) {
  display::test::TestScreen test_screen(/*create_display=*/false,
                                        /*register_screen=*/false);
  ScopedScreenOverride screen_override(&test_screen);

  display::Display display1(1, gfx::Rect(0, 0, 1920, 1080));
  // Use a top work area offset (50) safely below the native macOS menu bar
  // (which can be up to ~38pt on MacBook screens with a notch) so AppKit does
  // not constrain the native window below work_area.origin().
  display1.set_work_area(gfx::Rect(0, 50, 1920, 1030));
  test_screen.display_list().AddDisplay(display1,
                                        display::DisplayList::Type::PRIMARY);

  auto ui_manager = CreateUIManager();
  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* widget = ui_manager->widget();
  ASSERT_TRUE(widget);

  const gfx::Rect work_area = display1.work_area();
  const int widget_width = widget->GetWindowBoundsInScreen().width();
  const int widget_height = widget->GetWindowBoundsInScreen().height();

  // Simulate dragging the widget past the right/bottom edge of the work area.
  gfx::Rect offscreen_right_bottom(work_area.right() - 50,
                                   work_area.bottom() - 50, widget_width,
                                   widget_height);
  widget->SetBounds(offscreen_right_bottom);
  EXPECT_FALSE(work_area.Contains(widget->GetWindowBoundsInScreen()));

  // Ending the user drag clamps bounds back within the work area.
  ui_manager->OnWidgetUserDragStarted(widget);
  ui_manager->OnWidgetUserDragEnded(widget);

  EXPECT_TRUE(work_area.Contains(widget->GetWindowBoundsInScreen()));
  EXPECT_EQ(widget->GetWindowBoundsInScreen().right(), work_area.right());
  EXPECT_EQ(widget->GetWindowBoundsInScreen().bottom(), work_area.bottom());

  // Simulate dragging the widget past the top/left edge.
  gfx::Rect offscreen_top_left(work_area.x() - 100, work_area.y() - 100,
                               widget_width, widget_height);
  widget->SetBounds(offscreen_top_left);
  EXPECT_FALSE(work_area.Contains(widget->GetWindowBoundsInScreen()));

  ui_manager->OnWidgetUserDragStarted(widget);
  ui_manager->OnWidgetUserDragEnded(widget);

  EXPECT_TRUE(work_area.Contains(widget->GetWindowBoundsInScreen()));
  EXPECT_EQ(widget->GetWindowBoundsInScreen().origin(), work_area.origin());

  ui_manager->Shutdown();
}

TEST_F(OmniboxEverywhereUIManagerTest,
       RebuildWidgetOnEphemeralModelPrefChangeWhenVisible) {
  if (g_browser_process && g_browser_process->local_state()) {
    g_browser_process->local_state()->SetBoolean(
        omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel, false);
  }
  auto ui_manager = CreateUIManager();

  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* original_widget = ui_manager->widget();
  ASSERT_TRUE(original_widget);
  EXPECT_TRUE(original_widget->IsVisible());

  // Changing the ephemeral model pref should rebuild the widget.
  if (g_browser_process && g_browser_process->local_state()) {
    g_browser_process->local_state()->SetBoolean(
        omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel, true);
  }
  views::Widget* rebuilt_widget = ui_manager->widget();
  ASSERT_TRUE(rebuilt_widget);
  EXPECT_TRUE(rebuilt_widget->IsVisible());
  EXPECT_NE(original_widget, rebuilt_widget);

  ui_manager->Shutdown();
}

TEST_F(OmniboxEverywhereUIManagerTest,
       RebuildWidgetOnEphemeralModelPrefChangeWhenHidden) {
  if (g_browser_process && g_browser_process->local_state()) {
    g_browser_process->local_state()->SetBoolean(
        omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel, false);
  }
  auto ui_manager = CreateUIManager();

  ui_manager->ShowForProfile(&profile_, GetContext());
  ASSERT_TRUE(ui_manager->widget());
  EXPECT_TRUE(ui_manager->widget()->IsVisible());

  // Hide the widget.
  ui_manager->Close();
  EXPECT_FALSE(ui_manager->widget()->IsVisible());

  // Changing the ephemeral pref while hidden should clean up the old widget.
  if (g_browser_process && g_browser_process->local_state()) {
    g_browser_process->local_state()->SetBoolean(
        omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel, true);
  }
  EXPECT_FALSE(ui_manager->widget());

  // Showing again creates a new widget with the updated ephemeral settings.
  ui_manager->ShowForProfile(&profile_, GetContext());
  ASSERT_TRUE(ui_manager->widget());
  EXPECT_TRUE(ui_manager->widget()->IsVisible());

  ui_manager->Shutdown();
}

TEST_F(OmniboxEverywhereUIManagerTest,
       CleanUpWidgetOnMostVisitedPrefChangeWhenHidden) {
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpCustomLinksVisible, true);
  auto ui_manager = CreateUIManager();

  ui_manager->ShowForProfile(&profile_, GetContext());
  ASSERT_TRUE(ui_manager->widget());
  EXPECT_TRUE(ui_manager->widget()->IsVisible());

  // Hide the widget.
  ui_manager->Close();
  EXPECT_FALSE(ui_manager->widget()->IsVisible());
  EXPECT_TRUE(ui_manager->widget());

  // Changing a most-visited pref while hidden should clean up the old widget to
  // prevent stale frame buffer and tile flicker upon reopen.
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpCustomLinksVisible, false);
  EXPECT_FALSE(ui_manager->widget());

  // Showing again creates a fresh widget with the updated preferences.
  ui_manager->ShowForProfile(&profile_, GetContext());
  ASSERT_TRUE(ui_manager->widget());
  EXPECT_TRUE(ui_manager->widget()->IsVisible());

  ui_manager->Shutdown();
}

TEST_F(OmniboxEverywhereUIManagerTest,
       KeepWidgetOnMostVisitedPrefChangeWhenVisible) {
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpCustomLinksVisible, true);
  auto ui_manager = CreateUIManager();

  ui_manager->ShowForProfile(&profile_, GetContext());
  ASSERT_TRUE(ui_manager->widget());
  EXPECT_TRUE(ui_manager->widget()->IsVisible());

  // Changing a most-visited pref while visible should keep the widget alive for
  // in-place dynamic update via Mojo.
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpCustomLinksVisible, false);
  EXPECT_TRUE(ui_manager->widget());
  EXPECT_TRUE(ui_manager->widget()->IsVisible());

  ui_manager->Shutdown();
}

TEST_F(OmniboxEverywhereUIManagerTest,
       CleanUpWidgetOnOmniboxEverywhereShowShortcutsPrefChangeWhenHidden) {
  auto ui_manager = CreateUIManager();

  ui_manager->ShowForProfile(&profile_, GetContext());
  ASSERT_TRUE(ui_manager->widget());
  EXPECT_TRUE(ui_manager->widget()->IsVisible());

  // Hide the widget.
  ui_manager->Close();
  EXPECT_FALSE(ui_manager->widget()->IsVisible());
  EXPECT_TRUE(ui_manager->widget());

  // Changing profile show shortcuts pref while hidden should clean up the
  // old widget.
  profile_.GetPrefs()->SetInteger(
      omnibox_everywhere::prefs::kOmniboxEverywhereShowShortcuts,
      std::to_underlying(
          omnibox_everywhere::prefs::ShowShortcutsPrefValue::kDisabled));
  EXPECT_FALSE(ui_manager->widget());

  ui_manager->Shutdown();
}

TEST_F(OmniboxEverywhereUIManagerTest,
       CleanUpWidgetOnCustomLinksListPrefChangeWhenHidden) {
  auto ui_manager = CreateUIManager();

  ui_manager->ShowForProfile(&profile_, GetContext());
  ASSERT_TRUE(ui_manager->widget());
  EXPECT_TRUE(ui_manager->widget()->IsVisible());

  // Hide the widget.
  ui_manager->Close();
  EXPECT_FALSE(ui_manager->widget()->IsVisible());
  EXPECT_TRUE(ui_manager->widget());

  // Updating custom links (e.g. shortcut added, deleted, or reordered) while
  // hidden should clean up the old widget to prevent stale frame buffer and
  // tile flicker upon reopen.
  {
    ScopedListPrefUpdate update(profile_.GetPrefs(),
                                ntp_tiles::prefs::kCustomLinksList);
    update->Append("https://example.com");
  }
  EXPECT_FALSE(ui_manager->widget());

  ui_manager->Shutdown();
}

TEST_F(OmniboxEverywhereUIManagerTest, ScreensharePickerStateTracking) {
  auto ui_manager = CreateUIManager();
  ui_manager->ShowForProfile(&profile_, GetContext());
  ASSERT_TRUE(ui_manager->web_contents());

  EXPECT_FALSE(ui_manager->is_screenshare_picker_open_for_testing());
  EXPECT_FALSE(ui_manager->IsScreenshareCaptureInProgress());
  EXPECT_FALSE(ui_manager->web_contents()->ShouldIgnoreInputEventsForTesting());

  ui_manager->OnScreensharePickerOpened();
  EXPECT_TRUE(ui_manager->is_screenshare_picker_open_for_testing());
  EXPECT_TRUE(ui_manager->IsScreenshareCaptureInProgress());
  EXPECT_TRUE(ui_manager->web_contents()->ShouldIgnoreInputEventsForTesting());

  ui_manager->OnScreensharePickerClosed();
  EXPECT_FALSE(ui_manager->is_screenshare_picker_open_for_testing());
  EXPECT_FALSE(ui_manager->IsScreenshareCaptureInProgress());
  EXPECT_FALSE(ui_manager->web_contents()->ShouldIgnoreInputEventsForTesting());
}

TEST_F(OmniboxEverywhereUIManagerTest,
       CancelChromeDefaultPickerRestoresWidget) {
  auto ui_manager = CreateUIManager();
  ui_manager->ShowForProfile(&profile_, GetContext());
  ASSERT_TRUE(ui_manager->widget());
  EXPECT_TRUE(ui_manager->widget()->IsVisible());

  ui_manager->OnScreensharePickerOpened();
  EXPECT_TRUE(ui_manager->is_screenshare_picker_open_for_testing());
  EXPECT_TRUE(ui_manager->IsScreenshareCaptureInProgress());
  EXPECT_FALSE(ui_manager->widget()->IsVisible());

  EXPECT_TRUE(ui_manager->CancelChromeDefaultPicker());
  EXPECT_FALSE(ui_manager->is_screenshare_picker_open_for_testing());
  EXPECT_FALSE(ui_manager->IsScreenshareCaptureInProgress());
  EXPECT_TRUE(ui_manager->widget()->IsVisible());
}

TEST_F(OmniboxEverywhereUIManagerTest,
       CancelChromeDefaultPickerReturnsFalseWhenNotOpen) {
  auto ui_manager = CreateUIManager();
  ui_manager->ShowForProfile(&profile_, GetContext());
  ASSERT_TRUE(ui_manager->widget());
  EXPECT_TRUE(ui_manager->widget()->IsVisible());

  EXPECT_FALSE(ui_manager->CancelChromeDefaultPicker());
  EXPECT_TRUE(ui_manager->widget()->IsVisible());
}

TEST_F(OmniboxEverywhereUIManagerTest,
       CancelChromeDefaultPickerSuppressesRestoreWhenTargetProfileDiffers) {
  TestingProfile other_profile;
  auto ui_manager = CreateUIManager();
  ui_manager->ShowForProfile(&profile_, GetContext());
  ASSERT_TRUE(ui_manager->widget());
  EXPECT_TRUE(ui_manager->widget()->IsVisible());

  ui_manager->OnScreensharePickerOpened();
  EXPECT_TRUE(ui_manager->is_screenshare_picker_open_for_testing());
  EXPECT_TRUE(ui_manager->IsScreenshareCaptureInProgress());
  EXPECT_FALSE(ui_manager->widget()->IsVisible());

  // When invoked for a different profile, the picker is cancelled but the old
  // profile's widget is not restored/focused (preventing visual flicker).
  EXPECT_TRUE(ui_manager->CancelChromeDefaultPicker(&other_profile));
  EXPECT_FALSE(ui_manager->is_screenshare_picker_open_for_testing());
  EXPECT_FALSE(ui_manager->IsScreenshareCaptureInProgress());
  EXPECT_FALSE(ui_manager->widget()->IsVisible());
}

TEST_F(OmniboxEverywhereUIManagerTest, DismissBypassedDuringScreensharePicker) {
  if (g_browser_process && g_browser_process->local_state()) {
    g_browser_process->local_state()->SetBoolean(
        omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel, true);
  }
  auto ui_manager = CreateUIManager();

  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* widget = ui_manager->widget();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());

  // Mark screenshare picker as open. Opening the screenshare picker hides the
  // widget to prevent it from obstructing screen capture.
  ui_manager->OnScreensharePickerOpened();
  EXPECT_TRUE(ui_manager->is_screenshare_picker_open_for_testing());
  EXPECT_FALSE(widget->IsVisible());

  // Simulating deactivation while screenshare picker is open should NOT destroy
  // the widget.
  ui_manager->OnWidgetActivationChanged(widget, /*active=*/false);
  EXPECT_TRUE(ui_manager->widget());
  EXPECT_FALSE(widget->IsVisible());

  // Closing screenshare picker restores and activates the widget.
  ui_manager->OnScreensharePickerClosed();
  EXPECT_TRUE(ui_manager->widget());
  EXPECT_TRUE(widget->IsVisible());

  // Clean up: triggering deactivation after grace period should hide the widget
  // in ephemeral mode.
  task_environment()->FastForwardBy(
      omnibox_everywhere::OmniboxEverywhereUIManager::kActivationGracePeriod +
      base::Milliseconds(1));
  ui_manager->OnWidgetActivationChanged(widget, /*active=*/false);
  EXPECT_TRUE(base::test::RunUntil([&]() { return !widget->IsVisible(); }));

  ui_manager->Shutdown();
}

TEST_F(OmniboxEverywhereUIManagerTest,
       DismissBypassedDuringScreenshareDisclosure) {
  if (g_browser_process && g_browser_process->local_state()) {
    g_browser_process->local_state()->SetBoolean(
        omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel, true);
  }
  auto ui_manager = CreateUIManager();

  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* widget = ui_manager->widget();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());

  // Show screenshare disclosure dialog.
  ui_manager->ShowScreenshotDisclosureDialog(base::DoNothing());
  EXPECT_TRUE(ui_manager->is_screenshare_disclosure_open_for_testing());
  EXPECT_TRUE(ui_manager->HasOpenModalDialog());
  views::Widget* disclosure_widget =
      ui_manager->disclosure_dialog_widget_for_testing();
  ASSERT_TRUE(disclosure_widget);

  // Simulating deactivation while screenshare disclosure dialog is open
  // should NOT destroy or hide the widget.
  ui_manager->OnWidgetActivationChanged(widget, /*active=*/false);
  EXPECT_TRUE(ui_manager->widget());
  EXPECT_TRUE(widget->IsVisible());

  // Closing screenshare disclosure restores active state.
  views::test::WidgetDestroyedWaiter waiter(disclosure_widget);
  disclosure_widget->Close();
  waiter.Wait();
  EXPECT_TRUE(ui_manager->widget());
  EXPECT_TRUE(widget->IsVisible());
}

TEST_F(OmniboxEverywhereUIManagerTest, ScreenshareDisclosure_AcceptFlow) {
  auto ui_manager = CreateUIManager();
  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* widget = ui_manager->widget();
  ASSERT_TRUE(widget);
  ASSERT_TRUE(ui_manager->web_contents());
  EXPECT_FALSE(ui_manager->web_contents()->ShouldIgnoreInputEventsForTesting());

  base::test::TestFuture<void> accepted_future;
  base::test::TestFuture<void> cancelled_future;
  ui_manager->ShowScreenshotDisclosureDialog(accepted_future.GetCallback(),
                                             cancelled_future.GetCallback());

  views::Widget* disclosure_widget =
      ui_manager->disclosure_dialog_widget_for_testing();
  ASSERT_TRUE(disclosure_widget);
  EXPECT_TRUE(ui_manager->web_contents()->ShouldIgnoreInputEventsForTesting());
  views::DialogDelegate* delegate =
      disclosure_widget->widget_delegate()->AsDialogDelegate();
  ASSERT_TRUE(delegate);

  views::test::WidgetDestroyedWaiter waiter(disclosure_widget);
  delegate->AcceptDialog();
  waiter.Wait();

  EXPECT_TRUE(accepted_future.Wait());
  EXPECT_FALSE(cancelled_future.IsReady());
  EXPECT_FALSE(ui_manager->is_screenshare_disclosure_open_for_testing());
  EXPECT_FALSE(ui_manager->HasOpenModalDialog());
  EXPECT_FALSE(ui_manager->web_contents()->ShouldIgnoreInputEventsForTesting());
}

TEST_F(OmniboxEverywhereUIManagerTest, ScreenshareDisclosure_CancelFlow) {
  auto ui_manager = CreateUIManager();
  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* widget = ui_manager->widget();
  ASSERT_TRUE(widget);
  ASSERT_TRUE(ui_manager->web_contents());
  EXPECT_FALSE(ui_manager->web_contents()->ShouldIgnoreInputEventsForTesting());

  base::test::TestFuture<void> accepted_future;
  base::test::TestFuture<void> cancelled_future;
  ui_manager->ShowScreenshotDisclosureDialog(accepted_future.GetCallback(),
                                             cancelled_future.GetCallback());

  views::Widget* disclosure_widget =
      ui_manager->disclosure_dialog_widget_for_testing();
  ASSERT_TRUE(disclosure_widget);
  EXPECT_TRUE(ui_manager->is_screenshare_disclosure_open_for_testing());
  EXPECT_TRUE(ui_manager->HasOpenModalDialog());
  EXPECT_TRUE(ui_manager->web_contents()->ShouldIgnoreInputEventsForTesting());
  EXPECT_EQ(disclosure_widget->GetZOrderLevel(), widget->GetZOrderLevel());
  views::DialogDelegate* delegate =
      disclosure_widget->widget_delegate()->AsDialogDelegate();
  ASSERT_TRUE(delegate);

  views::test::WidgetDestroyedWaiter waiter(disclosure_widget);
  delegate->CancelDialog();
  waiter.Wait();

  EXPECT_FALSE(accepted_future.IsReady());
  EXPECT_TRUE(cancelled_future.Wait());
  EXPECT_FALSE(ui_manager->is_screenshare_disclosure_open_for_testing());
  EXPECT_FALSE(ui_manager->HasOpenModalDialog());
  EXPECT_FALSE(ui_manager->web_contents()->ShouldIgnoreInputEventsForTesting());
}

TEST_F(OmniboxEverywhereUIManagerTest,
       ScreenshareDisclosure_ShutdownWhileOpen) {
  auto ui_manager = CreateUIManager();
  ui_manager->ShowForProfile(&profile_, GetContext());

  bool accepted = false;
  bool cancelled = false;
  ui_manager->ShowScreenshotDisclosureDialog(
      base::BindOnce([](bool* a) { *a = true; }, &accepted),
      base::BindOnce([](bool* c) { *c = true; }, &cancelled));

  EXPECT_TRUE(ui_manager->disclosure_dialog_widget_for_testing());

  ui_manager->Shutdown();

  EXPECT_FALSE(accepted);
  EXPECT_FALSE(cancelled);
  EXPECT_FALSE(ui_manager->widget());
  EXPECT_FALSE(ui_manager->disclosure_dialog_widget_for_testing());
  EXPECT_FALSE(ui_manager->is_screenshare_disclosure_open_for_testing());
}

TEST_F(OmniboxEverywhereUIManagerTest,
       AcquiresAndReleasesKeepAliveOnWidgetLifecycle) {
  if (g_browser_process && g_browser_process->local_state()) {
    g_browser_process->local_state()->SetBoolean(
        omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel, false);
  }
  auto ui_manager = CreateUIManager();

  EXPECT_FALSE(KeepAliveRegistry::GetInstance()->IsOriginRegistered(
      KeepAliveOrigin::OMNIBOX_EVERYWHERE_UI));

  ui_manager->ShowForProfile(&profile_, GetContext());
  ASSERT_TRUE(ui_manager->widget());
  EXPECT_TRUE(ui_manager->widget()->IsVisible());
  EXPECT_TRUE(KeepAliveRegistry::GetInstance()->IsOriginRegistered(
      KeepAliveOrigin::OMNIBOX_EVERYWHERE_UI));

  // Closing the widget releases the keep-alive.
  ui_manager->Close();
  EXPECT_TRUE(base::test::RunUntil([]() {
    return !KeepAliveRegistry::GetInstance()->IsOriginRegistered(
        KeepAliveOrigin::OMNIBOX_EVERYWHERE_UI);
  }));

  // Showing again re-acquires the keep-alive.
  ui_manager->ShowForProfile(&profile_, GetContext());
  EXPECT_TRUE(KeepAliveRegistry::GetInstance()->IsOriginRegistered(
      KeepAliveOrigin::OMNIBOX_EVERYWHERE_UI));

  // Shutdown cleans up the widget and releases keep-alive.
  ui_manager->Shutdown();
  EXPECT_FALSE(KeepAliveRegistry::GetInstance()->IsOriginRegistered(
      KeepAliveOrigin::OMNIBOX_EVERYWHERE_UI));
}

#if BUILDFLAG(IS_WIN)
TEST_F(OmniboxEverywhereUIManagerTest, WindowPropertiesEphemeralMode) {
  if (g_browser_process && g_browser_process->local_state()) {
    g_browser_process->local_state()->SetBoolean(
        omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel, true);
  }
  auto ui_manager = CreateUIManager();
  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* widget = ui_manager->widget();
  ASSERT_TRUE(widget);

  HWND hwnd = views::HWNDForWidget(widget);
  ASSERT_NE(hwnd, nullptr);

  Microsoft::WRL::ComPtr<IPropertyStore> pps;
  ASSERT_HRESULT_SUCCEEDED(
      SHGetPropertyStoreForWindow(hwnd, IID_PPV_ARGS(&pps)));

  base::win::ScopedPropVariant pv;
  ASSERT_HRESULT_SUCCEEDED(
      pps->GetValue(PKEY_AppUserModel_PreventPinning, pv.Receive()));
  EXPECT_EQ(pv.get().vt, VT_BOOL);
  EXPECT_EQ(pv.get().boolVal, VARIANT_TRUE);

  ui_manager->Shutdown();
}

TEST_F(OmniboxEverywhereUIManagerTest, WindowPropertiesPersistentMode) {
  if (g_browser_process && g_browser_process->local_state()) {
    g_browser_process->local_state()->SetBoolean(
        omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel, false);
  }
  auto ui_manager = CreateUIManager();
  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* widget = ui_manager->widget();
  ASSERT_TRUE(widget);

  HWND hwnd = views::HWNDForWidget(widget);
  ASSERT_NE(hwnd, nullptr);

  Microsoft::WRL::ComPtr<IPropertyStore> pps;
  ASSERT_HRESULT_SUCCEEDED(
      SHGetPropertyStoreForWindow(hwnd, IID_PPV_ARGS(&pps)));

  // Verify AppUserModelID.
  base::win::ScopedPropVariant pv_appid;
  ASSERT_HRESULT_SUCCEEDED(
      pps->GetValue(PKEY_AppUserModel_ID, pv_appid.Receive()));
  EXPECT_EQ(pv_appid.get().vt, VT_LPWSTR);
  EXPECT_NE(
      std::wstring(pv_appid.get().pwszVal).find(L"app_search_with_chrome"),
      std::wstring::npos);

  // Verify RelaunchCommand.
  base::win::ScopedPropVariant pv_relaunch;
  ASSERT_HRESULT_SUCCEEDED(
      pps->GetValue(PKEY_AppUserModel_RelaunchCommand, pv_relaunch.Receive()));
  EXPECT_EQ(pv_relaunch.get().vt, VT_LPWSTR);
  std::wstring relaunch_command = pv_relaunch.get().pwszVal;
  EXPECT_NE(relaunch_command.find(L"chrome_proxy.exe"), std::wstring::npos);
  EXPECT_NE(relaunch_command.find(L"--omnibox-everywhere"), std::wstring::npos);

  // Verify WS_MINIMIZEBOX is present on the window style.
  EXPECT_NE(0, ::GetWindowLong(hwnd, GWL_STYLE) & WS_MINIMIZEBOX);

  ui_manager->Shutdown();
}

TEST_F(OmniboxEverywhereUIManagerTest,
       SysCommandMinimizeMinimizesInPersistentMode) {
  ASSERT_TRUE(g_browser_process);
  ASSERT_TRUE(g_browser_process->local_state());
  g_browser_process->local_state()->SetBoolean(
      omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel, false);

  auto ui_manager = CreateUIManager();
  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* widget = ui_manager->widget();
  ASSERT_TRUE(widget);
  views::test::WaitForWidgetActive(widget, true);
  EXPECT_TRUE(ui_manager->IsActive());
  EXPECT_FALSE(widget->IsMinimized());

  HWND hwnd = views::HWNDForWidget(widget);
  ASSERT_NE(hwnd, nullptr);

  // In persistent mode nothing intercepts the system command, so the window
  // minimizes natively.
  ::SendMessage(hwnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);
  EXPECT_TRUE(::IsIconic(hwnd));
  EXPECT_TRUE(widget->IsMinimized());
  EXPECT_FALSE(ui_manager->IsActive());
  EXPECT_FALSE(ui_manager->IsVisible());

  // Showing/reactivating restores the widget.
  ui_manager->ShowForProfile(&profile_, GetContext());
  views::test::WaitForWidgetActive(widget, true);
  EXPECT_FALSE(widget->IsMinimized());
  EXPECT_TRUE(ui_manager->IsVisible());
  EXPECT_TRUE(ui_manager->IsActive());

  ui_manager->Shutdown();
}

TEST_F(OmniboxEverywhereUIManagerTest,
       ShowWindowMinimizeClosesWidgetInEphemeralMode) {
  ASSERT_TRUE(g_browser_process);
  ASSERT_TRUE(g_browser_process->local_state());
  g_browser_process->local_state()->SetBoolean(
      omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel, true);

  auto ui_manager = CreateUIManager();
  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* widget = ui_manager->widget();
  ASSERT_TRUE(widget);
  HWND hwnd = views::HWNDForWidget(widget);
  ASSERT_NE(hwnd, nullptr);
  EXPECT_TRUE(ui_manager->IsVisible());

  // Exercise a minimize source that bypasses WM_SYSCOMMAND entirely (e.g.
  // Win+D, Aero Shake, or external ShowWindow) to verify
  // OnWidgetShowStateChanged covers non-SC_MINIMIZE paths.
  ::ShowWindow(hwnd, SW_MINIMIZE);
  EXPECT_FALSE(ui_manager->IsVisible());

  ui_manager->Shutdown();
}

TEST_F(OmniboxEverywhereUIManagerTest,
       SysCommandMinimizeAllowedWhileModalDialogOpen) {
  ASSERT_TRUE(g_browser_process);
  ASSERT_TRUE(g_browser_process->local_state());
  g_browser_process->local_state()->SetBoolean(
      omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel, false);

  auto ui_manager = CreateUIManager();
  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* widget = ui_manager->widget();
  ASSERT_TRUE(widget);
  HWND hwnd = views::HWNDForWidget(widget);
  ASSERT_NE(hwnd, nullptr);
  ASSERT_FALSE(::IsIconic(hwnd));

  ui_manager->OnDrivePickerOpened();
  ASSERT_TRUE(ui_manager->HasOpenModalDialog());

  // Native minimization is allowed while a modal dialog is open so the user
  // can minimize the window via the taskbar or window controls without the
  // widget or delegate being destroyed.
  constexpr WPARAM kReservedSysCommandBits = 0x0002;
  ::SendMessage(hwnd, WM_SYSCOMMAND, SC_MINIMIZE | kReservedSysCommandBits, 0);
  EXPECT_TRUE(::IsIconic(hwnd));
  EXPECT_TRUE(widget->IsMinimized());
  EXPECT_FALSE(ui_manager->IsVisible());
  EXPECT_TRUE(ui_manager->widget());
  EXPECT_TRUE(ui_manager->widget_delegate());

  ui_manager->ShowForProfile(&profile_, GetContext());
  views::test::WaitForWidgetActive(widget, true);
  EXPECT_FALSE(::IsIconic(hwnd));
  EXPECT_FALSE(widget->IsMinimized());
  EXPECT_TRUE(ui_manager->IsVisible());

  ui_manager->OnDrivePickerClosed();
  ui_manager->Shutdown();
}

TEST_F(OmniboxEverywhereUIManagerTest,
       MinimizeDoesNotCloseEphemeralWidgetWhileModalDialogOpen) {
  ASSERT_TRUE(g_browser_process);
  ASSERT_TRUE(g_browser_process->local_state());
  g_browser_process->local_state()->SetBoolean(
      omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel, true);

  auto ui_manager = CreateUIManager();
  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* widget = ui_manager->widget();
  ASSERT_TRUE(widget);
  ASSERT_TRUE(widget->IsVisible());

  ui_manager->OnDrivePickerOpened();
  ASSERT_TRUE(ui_manager->HasOpenModalDialog());

  // In ephemeral mode, minimizing while a modal dialog is open must allow
  // native minimization rather than calling Close() (which runs
  // CleanUpWidget() and destroys the widget and delegate owning the dialog).
  widget->Minimize();
  EXPECT_TRUE(widget->IsMinimized());
  EXPECT_FALSE(ui_manager->IsVisible());
  EXPECT_TRUE(ui_manager->widget());
  EXPECT_TRUE(ui_manager->widget_delegate());

  ui_manager->ShowForProfile(&profile_, GetContext());
  views::test::WaitForWidgetActive(widget, true);
  EXPECT_FALSE(widget->IsMinimized());
  EXPECT_TRUE(ui_manager->IsVisible());

  // Once the modal closes, minimizing in ephemeral mode really closes the
  // widget.
  ui_manager->OnDrivePickerClosed();
  ASSERT_FALSE(ui_manager->HasOpenModalDialog());
  ui_manager->widget()->Minimize();
  EXPECT_FALSE(ui_manager->IsVisible());
  EXPECT_FALSE(widget->IsVisible());

  ui_manager->Shutdown();
}

TEST_F(OmniboxEverywhereUIManagerTest, MinimizeClosesWidgetInEphemeralMode) {
  ASSERT_TRUE(g_browser_process);
  ASSERT_TRUE(g_browser_process->local_state());
  g_browser_process->local_state()->SetBoolean(
      omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel, true);

  auto ui_manager = CreateUIManager();
  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* widget = ui_manager->widget();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());

  // In ephemeral mode, minimizing (from any source) closes/hides the widget on
  // all platforms.
  widget->Minimize();
  EXPECT_FALSE(ui_manager->IsVisible());

  ui_manager->Shutdown();
}

TEST_F(OmniboxEverywhereUIManagerTest,
       DisableTaskbarPinningDoesNotAffectOpenWidget) {
  if (g_browser_process && g_browser_process->local_state()) {
    g_browser_process->local_state()->SetBoolean(
        omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel, false);
  }
  auto ui_manager = CreateUIManager();
  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* widget = ui_manager->widget();
  ASSERT_TRUE(widget);

  HWND hwnd = views::HWNDForWidget(widget);
  ASSERT_NE(hwnd, nullptr);

  // The check reports that no usable shortcut could be obtained.
  ui_manager->DisableTaskbarPinning();

  // The Shell ignores PreventPinning once the AUMID is set, so the open widget
  // keeps its pinning state; suppression applies to later widgets.
  Microsoft::WRL::ComPtr<IPropertyStore> pps;
  ASSERT_HRESULT_SUCCEEDED(
      SHGetPropertyStoreForWindow(hwnd, IID_PPV_ARGS(&pps)));
  base::win::ScopedPropVariant pv;
  ASSERT_HRESULT_SUCCEEDED(
      pps->GetValue(PKEY_AppUserModel_PreventPinning, pv.Receive()));
  EXPECT_EQ(pv.get().vt, VT_EMPTY);

  ui_manager->Shutdown();
}

TEST_F(OmniboxEverywhereUIManagerTest,
       DisableTaskbarPinningAppliesToLaterWidgets) {
  if (g_browser_process && g_browser_process->local_state()) {
    g_browser_process->local_state()->SetBoolean(
        omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel, false);
  }
  auto ui_manager = CreateUIManager();
  ASSERT_FALSE(ui_manager->widget());

  ui_manager->DisableTaskbarPinning();

  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* widget = ui_manager->widget();
  ASSERT_TRUE(widget);

  HWND hwnd = views::HWNDForWidget(widget);
  ASSERT_NE(hwnd, nullptr);

  Microsoft::WRL::ComPtr<IPropertyStore> pps;
  ASSERT_HRESULT_SUCCEEDED(
      SHGetPropertyStoreForWindow(hwnd, IID_PPV_ARGS(&pps)));
  base::win::ScopedPropVariant pv;
  ASSERT_HRESULT_SUCCEEDED(
      pps->GetValue(PKEY_AppUserModel_PreventPinning, pv.Receive()));
  EXPECT_EQ(pv.get().vt, VT_BOOL);
  EXPECT_EQ(pv.get().boolVal, VARIANT_TRUE);

  // Suppressing pinning must not cost the widget its own taskbar grouping and
  // icon, which the AppUserModelId provides.
  base::win::ScopedPropVariant pv_appid;
  ASSERT_HRESULT_SUCCEEDED(
      pps->GetValue(PKEY_AppUserModel_ID, pv_appid.Receive()));
  EXPECT_EQ(pv_appid.get().vt, VT_LPWSTR);
  EXPECT_NE(
      std::wstring(pv_appid.get().pwszVal).find(L"app_search_with_chrome"),
      std::wstring::npos);

  ui_manager->Shutdown();
}

TEST_F(OmniboxEverywhereUIManagerTest, CreateStartMenuShortcut) {
  auto ui_manager = CreateUIManager();
  base::test::TestFuture<bool> future;
  ui_manager->CreateStartMenuShortcut(future.GetCallback());
  EXPECT_TRUE(future.Get());
  ui_manager->Shutdown();
}

TEST_F(OmniboxEverywhereUIManagerTest, StartMenuShortcutRetriesOnFailure) {
  if (g_browser_process && g_browser_process->local_state()) {
    g_browser_process->local_state()->SetBoolean(
        omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel, false);
  }
  auto ui_manager = CreateUIManager();

  // Point DIR_START_MENU at a regular file so shortcut creation fails.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath not_a_directory =
      temp_dir.GetPath().Append(FILE_PATH_LITERAL("not_a_directory"));
  ASSERT_TRUE(base::WriteFile(not_a_directory, "content"));
  {
    base::ScopedPathOverride invalid_start_menu(base::DIR_START_MENU,
                                                not_a_directory,
                                                /*is_absolute=*/true,
                                                /*create=*/false);
    ui_manager->ShowForProfile(&profile_, GetContext());
    ASSERT_TRUE(ui_manager->widget());
    EXPECT_TRUE(base::test::RunUntil(
        [&]() { return ui_manager->taskbar_pinning_disabled_for_testing(); }));
    EXPECT_FALSE(ui_manager->start_menu_shortcut_requested_for_testing());
  }

  // Close the open widget so that a subsequent ShowForProfile creates a new
  // one.
  ui_manager->widget()->CloseNow();
  ASSERT_FALSE(ui_manager->widget());

  // With a valid Start Menu directory restored, creating the next persistent
  // widget retries shortcut creation and restores taskbar pinning.
  ui_manager->ShowForProfile(&profile_, GetContext());
  ASSERT_TRUE(ui_manager->widget());
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return !ui_manager->taskbar_pinning_disabled_for_testing(); }));
  EXPECT_TRUE(ui_manager->start_menu_shortcut_requested_for_testing());

  ui_manager->Shutdown();
}
#endif  // BUILDFLAG(IS_WIN)

TEST_F(OmniboxEverywhereUIManagerTest,
       ShowRegionSelectOverlay_CreateAndDismiss) {
  using RegionCaptureSource = OmniboxEverywhereUIManager::RegionCaptureSource;
  auto ui_manager = CreateUIManager();

  SkBitmap bitmap;
  bitmap.allocN32Pixels(100, 100);
  bitmap.eraseColor(SK_ColorRED);

  base::test::TestFuture<const SkBitmap&> future;
  ui_manager->ShowRegionSelectOverlay(
      bitmap, RegionCaptureSource::AllDisplays(), future.GetCallback());

  OmniboxEverywhereRegionSelectOverlay* overlay =
      ui_manager->region_select_overlay_for_testing();
  ASSERT_TRUE(overlay);
  views::Widget* overlay_widget = overlay->GetActiveWidgetForTesting();
  ASSERT_TRUE(overlay_widget);
  EXPECT_TRUE(overlay_widget->IsVisible());

  // Close the overlay widget (simulating Escape / dismiss).
  overlay_widget->CloseWithReason(views::Widget::ClosedReason::kEscKeyPressed);

  EXPECT_TRUE(future.IsReady());
  EXPECT_TRUE(future.Get().empty());
  EXPECT_FALSE(ui_manager->region_select_overlay_for_testing());

  ui_manager->Shutdown();
}

TEST_F(OmniboxEverywhereUIManagerTest,
       ShowRegionSelectOverlay_ReentrancyCancelsPreviousOverlay) {
  using RegionCaptureSource = OmniboxEverywhereUIManager::RegionCaptureSource;
  auto ui_manager = CreateUIManager();

  SkBitmap bitmap;
  bitmap.allocN32Pixels(100, 100);
  bitmap.eraseColor(SK_ColorRED);

  base::test::TestFuture<const SkBitmap&> future1;
  ui_manager->ShowRegionSelectOverlay(
      bitmap, RegionCaptureSource::AllDisplays(), future1.GetCallback());
  EXPECT_TRUE(ui_manager->region_select_overlay_for_testing());

  base::test::TestFuture<const SkBitmap&> future2;
  ui_manager->ShowRegionSelectOverlay(
      bitmap, RegionCaptureSource::AllDisplays(), future2.GetCallback());

  // The first overlay should be cancelled cleanly with empty bitmap.
  EXPECT_TRUE(future1.IsReady());
  EXPECT_TRUE(future1.Get().empty());
  EXPECT_FALSE(future2.IsReady());
  EXPECT_TRUE(ui_manager->region_select_overlay_for_testing());

  ui_manager->Shutdown();
  EXPECT_TRUE(future2.IsReady());
  EXPECT_TRUE(future2.Get().empty());
}

TEST_F(OmniboxEverywhereUIManagerTest,
       ShowRegionSelectOverlay_ShutdownOrCleanUpDismisses) {
  using RegionCaptureSource = OmniboxEverywhereUIManager::RegionCaptureSource;
  auto ui_manager = CreateUIManager();

  SkBitmap bitmap;
  bitmap.allocN32Pixels(100, 100);
  bitmap.eraseColor(SK_ColorRED);

  base::test::TestFuture<const SkBitmap&> future;
  ui_manager->ShowRegionSelectOverlay(
      bitmap, RegionCaptureSource::AllDisplays(), future.GetCallback());
  EXPECT_TRUE(ui_manager->region_select_overlay_for_testing());

  // CleanUpWidget / Shutdown cleanly destroys overlay and resolves callback.
  ui_manager->Shutdown();
  EXPECT_TRUE(future.IsReady());
  EXPECT_TRUE(future.Get().empty());
  EXPECT_FALSE(ui_manager->region_select_overlay_for_testing());
}

TEST_F(OmniboxEverywhereUIManagerTest, HasOpenModalDialog_RegionSelectOverlay) {
  using RegionCaptureSource = OmniboxEverywhereUIManager::RegionCaptureSource;
  auto ui_manager = CreateUIManager();
  ui_manager->ShowForProfile(&profile_, GetContext());
  ASSERT_TRUE(ui_manager->web_contents());

  EXPECT_FALSE(ui_manager->HasOpenModalDialog());
  EXPECT_FALSE(ui_manager->IsScreenshareCaptureInProgress());
  EXPECT_FALSE(ui_manager->web_contents()->ShouldIgnoreInputEventsForTesting());

  SkBitmap bitmap;
  bitmap.allocN32Pixels(100, 100);
  bitmap.eraseColor(SK_ColorRED);

  base::test::TestFuture<const SkBitmap&> future;
  ui_manager->ShowRegionSelectOverlay(
      bitmap, RegionCaptureSource::AllDisplays(), future.GetCallback());
  EXPECT_TRUE(ui_manager->HasOpenModalDialog());
  EXPECT_TRUE(ui_manager->IsScreenshareCaptureInProgress());
  EXPECT_TRUE(ui_manager->web_contents()->ShouldIgnoreInputEventsForTesting());

  ui_manager->region_select_overlay_for_testing()
      ->GetActiveWidgetForTesting()
      ->CloseWithReason(views::Widget::ClosedReason::kEscKeyPressed);
  EXPECT_TRUE(future.IsReady());
  EXPECT_FALSE(ui_manager->HasOpenModalDialog());
  EXPECT_FALSE(ui_manager->IsScreenshareCaptureInProgress());
  EXPECT_FALSE(ui_manager->web_contents()->ShouldIgnoreInputEventsForTesting());

  ui_manager->Shutdown();
}

TEST_F(OmniboxEverywhereUIManagerTest,
       CancelChromeDefaultPickerDoesNotCancelRegionSelectOverlay) {
  using RegionCaptureSource = OmniboxEverywhereUIManager::RegionCaptureSource;
  auto ui_manager = CreateUIManager();
  ui_manager->ShowForProfile(&profile_, GetContext());
  ASSERT_TRUE(ui_manager->web_contents());

  SkBitmap bitmap;
  bitmap.allocN32Pixels(100, 100);
  bitmap.eraseColor(SK_ColorRED);

  base::test::TestFuture<const SkBitmap&> future;
  ui_manager->ShowRegionSelectOverlay(
      bitmap, RegionCaptureSource::AllDisplays(), future.GetCallback());
  EXPECT_TRUE(ui_manager->HasOpenModalDialog());
  EXPECT_TRUE(ui_manager->IsScreenshareCaptureInProgress());

  // Region selection overlay covers the entire screen and is unaffected by
  // CancelChromeDefaultPicker().
  EXPECT_FALSE(ui_manager->CancelChromeDefaultPicker());
  EXPECT_FALSE(future.IsReady());
  EXPECT_TRUE(ui_manager->HasOpenModalDialog());
  EXPECT_TRUE(ui_manager->IsScreenshareCaptureInProgress());

  ui_manager->Shutdown();
  EXPECT_TRUE(future.IsReady());
  EXPECT_TRUE(future.Get().empty());
}

TEST_F(OmniboxEverywhereUIManagerTest,
       DismissBypassedDuringRegionSelectOverlay) {
  using RegionCaptureSource = OmniboxEverywhereUIManager::RegionCaptureSource;
  if (g_browser_process && g_browser_process->local_state()) {
    g_browser_process->local_state()->SetBoolean(
        omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel, true);
  }
  auto ui_manager = CreateUIManager();

  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* widget = ui_manager->widget();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());

  SkBitmap bitmap;
  bitmap.allocN32Pixels(100, 100);
  bitmap.eraseColor(SK_ColorRED);

  base::test::TestFuture<const SkBitmap&> future;
  ui_manager->ShowRegionSelectOverlay(
      bitmap, RegionCaptureSource::AllDisplays(), future.GetCallback());
  EXPECT_TRUE(ui_manager->HasOpenModalDialog());

  // Simulating deactivation while region select overlay is open should NOT
  // close the widget.
  ui_manager->OnWidgetActivationChanged(widget, /*active=*/false);
  EXPECT_TRUE(ui_manager->widget());
  EXPECT_TRUE(widget->IsVisible());

  // Dismiss overlay and simulate deactivation after grace period.
  ui_manager->region_select_overlay_for_testing()
      ->GetActiveWidgetForTesting()
      ->CloseWithReason(views::Widget::ClosedReason::kEscKeyPressed);
  EXPECT_TRUE(future.IsReady());
  EXPECT_FALSE(ui_manager->HasOpenModalDialog());

  task_environment()->FastForwardBy(
      omnibox_everywhere::OmniboxEverywhereUIManager::kActivationGracePeriod +
      base::Milliseconds(1));
  ui_manager->OnWidgetActivationChanged(widget, /*active=*/false);
  EXPECT_TRUE(base::test::RunUntil([&]() { return !widget->IsVisible(); }));

  ui_manager->Shutdown();
}

TEST_F(OmniboxEverywhereUIManagerTest, PermissionPromptStateTracking) {
  auto ui_manager = CreateUIManager();
  ui_manager->ShowForProfile(&profile_, GetContext());
  ASSERT_TRUE(ui_manager->web_contents());

  EXPECT_FALSE(ui_manager->is_permission_prompt_open_for_testing());
  EXPECT_FALSE(ui_manager->web_contents()->ShouldIgnoreInputEventsForTesting());

  ui_manager->OnPermissionPromptChanged(/*is_showing=*/true,
                                        gfx::Size(100, 100));
  EXPECT_TRUE(ui_manager->is_permission_prompt_open_for_testing());
  EXPECT_TRUE(ui_manager->web_contents()->ShouldIgnoreInputEventsForTesting());

  ui_manager->OnPermissionPromptChanged(/*is_showing=*/false, gfx::Size());
  EXPECT_FALSE(ui_manager->is_permission_prompt_open_for_testing());
  EXPECT_FALSE(ui_manager->web_contents()->ShouldIgnoreInputEventsForTesting());
}

TEST_F(OmniboxEverywhereUIManagerTest,
       HasOpenModalDialogUpdatesCorrectly_PermissionPrompt) {
  auto ui_manager = CreateUIManager();
  EXPECT_FALSE(ui_manager->HasOpenModalDialog());

  ui_manager->OnPermissionPromptChanged(/*is_showing=*/true,
                                        gfx::Size(100, 100));
  EXPECT_TRUE(ui_manager->HasOpenModalDialog());

  ui_manager->OnPermissionPromptChanged(/*is_showing=*/false, gfx::Size());
  EXPECT_FALSE(ui_manager->HasOpenModalDialog());
}

TEST_F(OmniboxEverywhereUIManagerTest, DismissBypassedDuringPermissionPrompt) {
  if (g_browser_process && g_browser_process->local_state()) {
    g_browser_process->local_state()->SetBoolean(
        omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel, true);
  }
  auto ui_manager = CreateUIManager();

  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* widget = ui_manager->widget();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());

  // Mark permission prompt as showing.
  ui_manager->OnPermissionPromptChanged(/*is_showing=*/true,
                                        gfx::Size(100, 100));
  EXPECT_TRUE(ui_manager->is_permission_prompt_open_for_testing());
  EXPECT_TRUE(ui_manager->HasOpenModalDialog());

  // Simulating deactivation while permission prompt is open should NOT close
  // the widget.
  ui_manager->OnWidgetActivationChanged(widget, /*active=*/false);
  EXPECT_TRUE(ui_manager->widget());
  EXPECT_TRUE(widget->IsVisible());

  // Mark permission prompt as closed and simulate deactivation after grace
  // period.
  ui_manager->OnPermissionPromptChanged(/*is_showing=*/false, gfx::Size());
  EXPECT_FALSE(ui_manager->is_permission_prompt_open_for_testing());
  EXPECT_FALSE(ui_manager->HasOpenModalDialog());

  task_environment()->FastForwardBy(
      omnibox_everywhere::OmniboxEverywhereUIManager::kActivationGracePeriod +
      base::Milliseconds(1));
  ui_manager->OnWidgetActivationChanged(widget, /*active=*/false);
  EXPECT_TRUE(base::test::RunUntil([&]() { return !widget->IsVisible(); }));

  ui_manager->Shutdown();
}

TEST_F(OmniboxEverywhereUIManagerTest, HotkeyDropdownStateTracking) {
  auto ui_manager = CreateUIManager();
  EXPECT_FALSE(ui_manager->is_hotkey_dropdown_open_for_testing());
  EXPECT_FALSE(ui_manager->HasOpenModalDialog());

  ui_manager->OnHotkeyDropdownOpened();
  EXPECT_TRUE(ui_manager->is_hotkey_dropdown_open_for_testing());
  EXPECT_TRUE(ui_manager->HasOpenModalDialog());

  ui_manager->OnHotkeyDropdownClosed();
  EXPECT_FALSE(ui_manager->is_hotkey_dropdown_open_for_testing());
  EXPECT_FALSE(ui_manager->HasOpenModalDialog());
}

TEST_F(OmniboxEverywhereUIManagerTest, DismissBypassedDuringHotkeyDropdown) {
  if (g_browser_process && g_browser_process->local_state()) {
    g_browser_process->local_state()->SetBoolean(
        omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel, true);
  }
  auto ui_manager = CreateUIManager();

  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* widget = ui_manager->widget();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());

  // Mark hotkey dropdown as open.
  ui_manager->OnHotkeyDropdownOpened();
  EXPECT_TRUE(ui_manager->is_hotkey_dropdown_open_for_testing());
  EXPECT_TRUE(ui_manager->HasOpenModalDialog());

  // Simulating deactivation while hotkey dropdown is open should NOT close
  // the widget.
  ui_manager->OnWidgetActivationChanged(widget, /*active=*/false);
  EXPECT_TRUE(ui_manager->widget());
  EXPECT_TRUE(widget->IsVisible());

  // Closing hotkey dropdown posts a deactivation check.
  ui_manager->OnHotkeyDropdownClosed();
  EXPECT_FALSE(ui_manager->is_hotkey_dropdown_open_for_testing());
  EXPECT_FALSE(ui_manager->HasOpenModalDialog());
  EXPECT_TRUE(
      ui_manager->is_hotkey_dropdown_deactivation_task_pending_for_testing());

  // Reopening the hotkey dropdown should cancel the pending deactivation task.
  ui_manager->OnHotkeyDropdownOpened();
  EXPECT_FALSE(
      ui_manager->is_hotkey_dropdown_deactivation_task_pending_for_testing());
  EXPECT_TRUE(ui_manager->is_hotkey_dropdown_open_for_testing());

  // Close it again, verify the pending task is scheduled, and run it.
  ui_manager->OnHotkeyDropdownClosed();
  EXPECT_TRUE(
      ui_manager->is_hotkey_dropdown_deactivation_task_pending_for_testing());
  task_environment()->FastForwardBy(base::Milliseconds(1));
  EXPECT_FALSE(
      ui_manager->is_hotkey_dropdown_deactivation_task_pending_for_testing());

  // Advance time past the grace period and simulate deactivation.
  task_environment()->FastForwardBy(
      omnibox_everywhere::OmniboxEverywhereUIManager::kActivationGracePeriod +
      base::Milliseconds(1));
  ui_manager->OnWidgetActivationChanged(widget, /*active=*/false);
  EXPECT_TRUE(base::test::RunUntil([&]() { return !widget->IsVisible(); }));

  ui_manager->Shutdown();
}

TEST_F(OmniboxEverywhereUIManagerTest,
       CapturerCountMaintainedWhileHiddenForAutoResize) {
  auto ui_manager = CreateUIManager();

  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* widget = ui_manager->widget();
  ASSERT_TRUE(widget);
  ASSERT_TRUE(ui_manager->web_contents());
  EXPECT_FALSE(ui_manager->web_contents()->IsBeingCaptured());

  // Simulate widget expanding due to autocomplete matches.
  ui_manager->ResizeDueToAutoResize(ui_manager->web_contents(),
                                    gfx::Size(728, 300));
  EXPECT_EQ(widget->GetSize().height(), 300);

  // Close hides the widget immediately while temporarily keeping the
  // WebContents captured (`kHiddenButPainting`) so Blink AutoResize continues
  // offscreen.
  ui_manager->Close();
  EXPECT_FALSE(widget->IsVisible());
  EXPECT_TRUE(ui_manager->web_contents()->IsBeingCaptured());
  EXPECT_FALSE(ui_manager->web_contents()->IsBeingVisiblyCaptured());

  // Simulate Blink AutoResize collapsing the dropdown back to resting height
  // (56px) while the widget is hidden.
  ui_manager->ResizeDueToAutoResize(ui_manager->web_contents(),
                                    gfx::Size(728, 56));
  EXPECT_EQ(widget->GetSize().height(), 56);

  // Once the post-hide capture window expires, the capture lock is released so
  // the WebContents enters `kHidden` with zero idle overhead.
  task_environment()->FastForwardBy(
      OmniboxEverywhereUIManager::kPostHideCaptureDuration);
  EXPECT_FALSE(ui_manager->web_contents()->IsBeingCaptured());
  EXPECT_EQ(widget->GetSize().height(), 56);

  // Re-showing the widget presents it at the collapsed height without flash.
  ui_manager->ShowForProfile(&profile_, GetContext());
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_EQ(widget->GetSize().height(), 56);

  ui_manager->Shutdown();
  EXPECT_FALSE(ui_manager->web_contents());
}

TEST_F(OmniboxEverywhereUIManagerTest, LensPromoLifecycleInPersistentMode) {
  if (g_browser_process && g_browser_process->local_state()) {
    g_browser_process->local_state()->SetBoolean(
        omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel, false);
  }
  auto* test_service = static_cast<TestingOmniboxEverywhereService*>(
      OmniboxEverywhereServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          &profile_, base::BindRepeating([](content::BrowserContext* context)
                                             -> std::unique_ptr<KeyedService> {
            return std::make_unique<TestingOmniboxEverywhereService>(
                Profile::FromBrowserContext(context));
          })));
  ASSERT_TRUE(test_service);

  auto ui_manager = CreateUIManager();

  // 1. Invoking LB shows the Lens promo once.
  ui_manager->ShowForProfile(&profile_, GetContext());
  views::Widget* widget = ui_manager->widget();
  ASSERT_TRUE(widget);
  EXPECT_EQ(1, test_service->maybe_show_lens_promo_count());
  EXPECT_EQ(0, test_service->end_lens_promo_count());
  EXPECT_FALSE(ui_manager->is_demoted());

  // 2. De-invoking via Demote() ends the promo impression.
  ui_manager->Demote();
  EXPECT_TRUE(ui_manager->is_demoted());
  EXPECT_EQ(1, test_service->end_lens_promo_count());

  // Calling Demote() again while already demoted is a no-op.
  ui_manager->Demote();
  EXPECT_EQ(1, test_service->end_lens_promo_count());

  // 3. Re-invoking via ShowForProfile starts a new impression check.
  ui_manager->ShowForProfile(&profile_, GetContext());
  EXPECT_FALSE(ui_manager->is_demoted());
  EXPECT_EQ(2, test_service->maybe_show_lens_promo_count());

  // 4. Demoting and then reactivating via OS click / taskbar activation
  // (OnWidgetActivationChanged) also starts a new impression check.
  ui_manager->Demote();
  EXPECT_TRUE(ui_manager->is_demoted());
  EXPECT_EQ(2, test_service->end_lens_promo_count());

  ui_manager->OnWidgetActivationChanged(widget, /*active=*/true);
  EXPECT_FALSE(ui_manager->is_demoted());
  EXPECT_EQ(3, test_service->maybe_show_lens_promo_count());

#if BUILDFLAG(IS_WIN)
  // 5. Minimizing the widget ends the promo impression and marks it demoted;
  // restoring it restarts the promo once.
  widget->Minimize();
  EXPECT_TRUE(ui_manager->is_demoted());
  EXPECT_EQ(3, test_service->end_lens_promo_count());

  widget->Restore();
  EXPECT_FALSE(ui_manager->is_demoted());
  EXPECT_EQ(4, test_service->maybe_show_lens_promo_count());

  // 6. Minimizing an already-demoted widget does not double-end the promo,
  // and restoring it still restarts the promo once.
  ui_manager->Demote();
  EXPECT_TRUE(ui_manager->is_demoted());
  EXPECT_EQ(4, test_service->end_lens_promo_count());

  widget->Minimize();
  EXPECT_TRUE(ui_manager->is_demoted());
  EXPECT_EQ(4, test_service->end_lens_promo_count());

  widget->Restore();
  EXPECT_FALSE(ui_manager->is_demoted());
  EXPECT_EQ(5, test_service->maybe_show_lens_promo_count());
#endif  // BUILDFLAG(IS_WIN)

  ui_manager->Shutdown();
}

TEST_F(OmniboxEverywhereUIManagerTest, LensPromoLifecycleInEphemeralMode) {
  if (g_browser_process && g_browser_process->local_state()) {
    g_browser_process->local_state()->SetBoolean(
        omnibox_everywhere::prefs::kOmniboxEverywhereEphemeralModel, true);
  }
  auto* test_service = static_cast<TestingOmniboxEverywhereService*>(
      OmniboxEverywhereServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          &profile_, base::BindRepeating([](content::BrowserContext* context)
                                             -> std::unique_ptr<KeyedService> {
            return std::make_unique<TestingOmniboxEverywhereService>(
                Profile::FromBrowserContext(context));
          })));
  ASSERT_TRUE(test_service);

  auto ui_manager = CreateUIManager();

  // Invoking LB shows the Lens promo.
  ui_manager->ShowForProfile(&profile_, GetContext());
  EXPECT_EQ(1, test_service->maybe_show_lens_promo_count());
  EXPECT_EQ(0, test_service->end_lens_promo_count());

  // Closing/hiding LB in ephemeral mode ends the promo impression once via
  // ReleaseKeepAlives().
  ui_manager->Close();
  EXPECT_EQ(1, test_service->end_lens_promo_count());

  // Re-invoking LB attempts to show the promo again as a fresh invocation.
  ui_manager->ShowForProfile(&profile_, GetContext());
  EXPECT_EQ(2, test_service->maybe_show_lens_promo_count());

  ui_manager->Shutdown();
}

}  // namespace omnibox_everywhere
