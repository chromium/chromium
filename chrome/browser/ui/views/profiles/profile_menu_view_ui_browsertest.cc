// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_menu_view.h"

#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/browser/ui/views/profiles/profile_menu_coordinator.h"
#include "chrome/browser/ui/views/profiles/profiles_pixel_test_utils.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/events/event_utils.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/startup/browser_init_params.h"
#endif

namespace {

enum class ProfileTypePixelTestParam {
  kRegular,
  kIncognito,
  kGuest,
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  kDeviceGuestSession,
#endif
};

struct ProfileMenuViewPixelTestParam {
  PixelTestParam pixel_test_param;
  ProfileTypePixelTestParam profile_type_param;
};

// To be passed as 4th argument to `INSTANTIATE_TEST_SUITE_P()`, allows the test
// to be named like `<TestClassName>.InvokeUi_default/<TestSuffix>` instead
// of using the index of the param in `TestParam` as suffix.
std::string ParamToTestSuffix(
    const ::testing::TestParamInfo<ProfileMenuViewPixelTestParam>& info) {
  return info.param.pixel_test_param.test_suffix;
}

// Permutations of supported parameters.
const ProfileMenuViewPixelTestParam kPixelTestParams[] = {
    // Legacy design (to be removed)
    {.pixel_test_param = {.test_suffix = "Regular"},
     .profile_type_param = ProfileTypePixelTestParam::kRegular},
    {.pixel_test_param = {.test_suffix = "Guest"},
     .profile_type_param = ProfileTypePixelTestParam::kGuest},
    {.pixel_test_param = {.test_suffix = "Incognito"},
     .profile_type_param = ProfileTypePixelTestParam::kIncognito},
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    {.pixel_test_param = {.test_suffix = "LacrosDeviceGuestSession"},
     .profile_type_param = ProfileTypePixelTestParam::kDeviceGuestSession},
#endif
    {.pixel_test_param = {.test_suffix = "DarkTheme", .use_dark_theme = true},
     .profile_type_param = ProfileTypePixelTestParam::kRegular},
    {.pixel_test_param = {.test_suffix = "RTL",
                          .use_right_to_left_language = true},
     .profile_type_param = ProfileTypePixelTestParam::kRegular},

    // CR2023 design
    {.pixel_test_param = {.test_suffix = "CR2023",
                          .use_chrome_refresh_2023_style = true},
     .profile_type_param = ProfileTypePixelTestParam::kRegular},
    {.pixel_test_param = {.test_suffix = "CR2023_Guest",
                          .use_chrome_refresh_2023_style = true},
     .profile_type_param = ProfileTypePixelTestParam::kGuest},
    {.pixel_test_param = {.test_suffix = "CR2023_DarkTheme_Guest",
                          .use_dark_theme = true,
                          .use_chrome_refresh_2023_style = true},
     .profile_type_param = ProfileTypePixelTestParam::kGuest},
    {.pixel_test_param = {.test_suffix = "CR2023_Incognito",
                          .use_chrome_refresh_2023_style = true},
     .profile_type_param = ProfileTypePixelTestParam::kIncognito},
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    {.pixel_test_param = {.test_suffix = "CR2023_LacrosDeviceGuestSession",
                          .use_chrome_refresh_2023_style = true},
     .profile_type_param = ProfileTypePixelTestParam::kDeviceGuestSession},
#endif
    {.pixel_test_param = {.test_suffix = "CR2023_DarkTheme",
                          .use_dark_theme = true,
                          .use_chrome_refresh_2023_style = true},
     .profile_type_param = ProfileTypePixelTestParam::kRegular},
    {.pixel_test_param = {.test_suffix = "CR2023_RTL",
                          .use_right_to_left_language = true,
                          .use_chrome_refresh_2023_style = true},
     .profile_type_param = ProfileTypePixelTestParam::kRegular},
};

}  // namespace

class ProfileMenuViewPixelTest
    : public ProfilesPixelTestBaseT<DialogBrowserTest>,
      public testing::WithParamInterface<ProfileMenuViewPixelTestParam> {
 public:
  ProfileMenuViewPixelTest()
      : ProfilesPixelTestBaseT<DialogBrowserTest>(GetParam().pixel_test_param) {
  }

  ~ProfileMenuViewPixelTest() override = default;

  ProfileTypePixelTestParam GetProfileType() const {
    return GetParam().profile_type_param;
  }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Enable the guest session.
  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    if (GetProfileType() != ProfileTypePixelTestParam::kDeviceGuestSession) {
      return;
    }
    crosapi::mojom::BrowserInitParamsPtr init_params =
        chromeos::BrowserInitParams::GetForTests()->Clone();

    init_params->session_type = crosapi::mojom::SessionType::kGuestSession;
    chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));
    ProfilesPixelTestBaseT<DialogBrowserTest>::CreatedBrowserMainParts(
        browser_main_parts);
  }
#endif

  void SetUpOnMainThread() override {
    ProfilesPixelTestBaseT<DialogBrowserTest>::SetUpOnMainThread();

    // Configures the browser according to the profile type.
    ui_test_utils::BrowserChangeObserver browser_added_observer(
        nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
    Browser* new_browser = nullptr;

    switch (GetProfileType()) {
      case ProfileTypePixelTestParam::kRegular:
        // Nothing to do.
        break;
      case ProfileTypePixelTestParam::kIncognito:
        CreateIncognitoBrowser();
        new_browser = browser_added_observer.Wait();
        ASSERT_TRUE(new_browser);
        ASSERT_TRUE(new_browser->profile()->IsIncognitoProfile());
        break;
      case ProfileTypePixelTestParam::kGuest:
        CreateGuestBrowser();
        new_browser = browser_added_observer.Wait();
        ASSERT_TRUE(new_browser);
        ASSERT_TRUE(new_browser->profile()->IsGuestSession());
        break;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
      case ProfileTypePixelTestParam::kDeviceGuestSession:
        // Nothing to do, the current browser should already be guest.
        ASSERT_TRUE(browser()->profile()->IsGuestSession());
#endif
    }

    // Close the initial browser and set the new one as default.
    if (new_browser) {
      ASSERT_NE(new_browser, browser());
      CloseBrowserSynchronously(browser());
      SelectFirstBrowser();
      ASSERT_EQ(new_browser, browser());
    }
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    CHECK(browser());

    views::NamedWidgetShownWaiter widget_waiter(
        views::test::AnyWidgetTestPasskey{}, "ProfileMenuViewBase");

    ASSERT_NO_FATAL_FAILURE(OpenProfileMenu());

    widget_waiter.WaitIfNeededAndGet();
  }

 private:
  void OpenProfileMenu() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    OpenProfileMenuFromToolbar(browser_view->toolbar_button_provider());
  }

  void OpenProfileMenuFromToolbar(ToolbarButtonProvider* toolbar) {
    // Click the avatar button to open the menu.
    views::View* avatar_button = toolbar->GetAvatarToolbarButton();
    ASSERT_TRUE(avatar_button);
    Click(avatar_button);

    ASSERT_TRUE(profile_menu_view());
    profile_menu_view()->set_close_on_deactivate(false);

#if BUILDFLAG(IS_MAC)
    base::RunLoop().RunUntilIdle();
#else
    // If possible wait until the menu is active.
    views::Widget* menu_widget = profile_menu_view()->GetWidget();
    ASSERT_TRUE(menu_widget);
    if (menu_widget->CanActivate()) {
      views::test::WidgetActivationWaiter(menu_widget, /*active=*/true).Wait();
    } else {
      LOG(ERROR) << "menu_widget can not be activated";
    }
#endif

    LOG(INFO) << "Opening profile menu was successful";
  }

  void Click(views::View* clickable_view) {
    // Simulate a mouse click. Note: Buttons are either fired when pressed or
    // when released, so the corresponding methods need to be called.
    clickable_view->OnMousePressed(
        ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
    clickable_view->OnMouseReleased(
        ui::MouseEvent(ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
  }

  ProfileMenuViewBase* profile_menu_view() {
    auto* coordinator = ProfileMenuCoordinator::FromBrowser(browser());
    return coordinator ? coordinator->GetProfileMenuViewBaseForTesting()
                       : nullptr;
  }
};

IN_PROC_BROWSER_TEST_P(ProfileMenuViewPixelTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(,
                         ProfileMenuViewPixelTest,
                         testing::ValuesIn(kPixelTestParams),
                         &ParamToTestSuffix);
