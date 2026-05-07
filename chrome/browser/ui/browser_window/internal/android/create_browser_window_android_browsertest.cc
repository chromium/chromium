// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/public/create_browser_window.h"

#include <utility>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/test/android/browser_window_android_browsertest_base.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "components/feed/feed_feature_list.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/mojom/window_show_state.mojom.h"

namespace {
class NavigationStartedObserver : public content::WebContentsObserver {
 public:
  explicit NavigationStartedObserver(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}

  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override {
    navigation_started_ = true;
  }

  bool navigation_started() const { return navigation_started_; }

 private:
  bool navigation_started_ = false;
};

BrowserWindowInterface* CreateBrowserWindowSync(
    BrowserWindowInterface::Type type,
    Profile* profile,
    ui::mojom::WindowShowState initial_show_state =
        ui::mojom::WindowShowState::kDefault) {
  BrowserWindowCreateParams create_params =
      BrowserWindowCreateParams(type, *profile, /*from_user_gesture=*/false);
  create_params.initial_show_state = initial_show_state;

  return CreateBrowserWindow(std::move(create_params));
}

BrowserWindowInterface* CreateBrowserWindowAsync(
    BrowserWindowInterface::Type type,
    Profile* profile,
    ui::mojom::WindowShowState initial_show_state =
        ui::mojom::WindowShowState::kDefault) {
  BrowserWindowCreateParams create_params =
      BrowserWindowCreateParams(type, *profile, /*from_user_gesture=*/false);
  create_params.initial_show_state = initial_show_state;

  base::test::TestFuture<BrowserWindowInterface*> future;
  CreateBrowserWindow(std::move(create_params), future.GetCallback());
  return future.Get();
}

void AssertBrowserWindow(BrowserWindowInterface* browser_window,
                         BrowserWindowInterface::Type expected_type,
                         Profile* expected_profile,
                         bool expect_fully_initialized = false) {
  ASSERT_NE(browser_window, nullptr);
  EXPECT_EQ(browser_window->GetType(), expected_type);
  EXPECT_EQ(browser_window->GetProfile(), expected_profile);

  if (expect_fully_initialized) {
    auto* tab_list_interface = TabListInterface::From(browser_window);
    ASSERT_NE(tab_list_interface, nullptr);
    EXPECT_EQ(tab_list_interface->GetTabCount(), 1);
    EXPECT_EQ(tab_list_interface->GetActiveIndex(), 0);
    ASSERT_NE(tab_list_interface->GetActiveTab(), nullptr);
  }
}
}  // namespace

class CreateBrowserWindowAndroidBrowserTest
    : public BrowserWindowAndroidBrowserTestBase {};

IN_PROC_BROWSER_TEST_F(
    CreateBrowserWindowAndroidBrowserTest,
    CreateBrowserWindowSync_RegularProfile_ReturnsRegularBrowserWindow) {
  auto type = BrowserWindowInterface::Type::TYPE_NORMAL;
  Profile* profile = GetProfile();

  BrowserWindowInterface* new_browser_window =
      CreateBrowserWindowSync(type, profile);

  AssertBrowserWindow(new_browser_window, type, profile);
}

IN_PROC_BROWSER_TEST_F(
    CreateBrowserWindowAndroidBrowserTest,
    CreateBrowserWindowSync_IncognitoProfile_ReturnsIncognitoBrowserWindow) {
  auto type = BrowserWindowInterface::Type::TYPE_NORMAL;
  Profile* incognito_profile =
      GetProfile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);

  BrowserWindowInterface* new_browser_window =
      CreateBrowserWindowSync(type, incognito_profile);

  AssertBrowserWindow(new_browser_window, type, incognito_profile);
}

IN_PROC_BROWSER_TEST_F(CreateBrowserWindowAndroidBrowserTest,
                       CreateBrowserWindowSync_Popup_ReturnsPopupWindow) {
  auto type = BrowserWindowInterface::Type::TYPE_POPUP;
  Profile* profile = GetProfile();

  BrowserWindowInterface* new_browser_window =
      CreateBrowserWindowSync(type, profile);

  AssertBrowserWindow(new_browser_window, type, profile);
}

IN_PROC_BROWSER_TEST_F(
    CreateBrowserWindowAndroidBrowserTest,
    CreateBrowserWindowSync_IncognitoPopup_ReturnsIncognitoPopupWindow) {
  auto type = BrowserWindowInterface::Type::TYPE_POPUP;
  Profile* incognito_profile =
      GetProfile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);

  BrowserWindowInterface* new_browser_window =
      CreateBrowserWindowSync(type, incognito_profile);

  AssertBrowserWindow(new_browser_window, type, incognito_profile);
}

IN_PROC_BROWSER_TEST_F(
    CreateBrowserWindowAndroidBrowserTest,
    CreateBrowserWindowSync_UnsupportedWindowType_ReturnsNull) {
  auto type =
      BrowserWindowInterface::Type::TYPE_APP;  // not supported on Android
  Profile* profile = GetProfile();

  BrowserWindowInterface* new_browser_window =
      CreateBrowserWindowSync(type, profile);

  EXPECT_EQ(new_browser_window, nullptr);
}

IN_PROC_BROWSER_TEST_F(
    CreateBrowserWindowAndroidBrowserTest,
    CreateBrowserWindowSync_UnsupportedInitialShowState_ReturnsNull) {
  auto type = BrowserWindowInterface::Type::TYPE_NORMAL;
  Profile* profile = GetProfile();
  auto initial_show_state =
      ui::mojom::WindowShowState::kFullscreen;  // not supported on Android

  BrowserWindowInterface* new_browser_window =
      CreateBrowserWindowSync(type, profile, initial_show_state);

  EXPECT_EQ(new_browser_window, nullptr);
}

IN_PROC_BROWSER_TEST_F(
    CreateBrowserWindowAndroidBrowserTest,
    CreateBrowserWindowAsync_RegularProfile_TriggersCallbackWithRegularBrowserWindow) {
  auto type = BrowserWindowInterface::Type::TYPE_NORMAL;
  Profile* profile = GetProfile();

  BrowserWindowInterface* new_browser_window =
      CreateBrowserWindowAsync(type, profile);

  AssertBrowserWindow(new_browser_window, type, profile,
                      /*expect_fully_initialized=*/true);
}

IN_PROC_BROWSER_TEST_F(
    CreateBrowserWindowAndroidBrowserTest,
    CreateBrowserWindowAsync_IncognitoProfile_TriggersCallbackWithIncognitoBrowserWindow) {
  auto type = BrowserWindowInterface::Type::TYPE_NORMAL;
  Profile* incognito_profile =
      GetProfile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);

  BrowserWindowInterface* new_browser_window =
      CreateBrowserWindowAsync(type, incognito_profile);

  AssertBrowserWindow(new_browser_window, type, incognito_profile,
                      /*expect_fully_initialized=*/true);
}

IN_PROC_BROWSER_TEST_F(
    CreateBrowserWindowAndroidBrowserTest,
    CreateBrowserWindowAsync_Popup_TriggersCallbackWithPopupWindow) {
  auto type = BrowserWindowInterface::Type::TYPE_POPUP;
  Profile* profile = GetProfile();

  BrowserWindowInterface* new_browser_window =
      CreateBrowserWindowAsync(type, profile);

  AssertBrowserWindow(new_browser_window, type, profile,
                      /*expect_fully_initialized=*/true);
}

IN_PROC_BROWSER_TEST_F(
    CreateBrowserWindowAndroidBrowserTest,
    CreateBrowserWindowAsync_IncognitoPopup_TriggersCallbackWithIncognitoPopupWindow) {
  auto type = BrowserWindowInterface::Type::TYPE_POPUP;
  Profile* incognito_profile =
      GetProfile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);

  BrowserWindowInterface* new_browser_window =
      CreateBrowserWindowAsync(type, incognito_profile);

  AssertBrowserWindow(new_browser_window, type, incognito_profile,
                      /*expect_fully_initialized=*/true);
}

IN_PROC_BROWSER_TEST_F(
    CreateBrowserWindowAndroidBrowserTest,
    CreateBrowserWindowAsync_UnsupportedWindowType_TriggersCallbackWithNull) {
  auto type =
      BrowserWindowInterface::Type::TYPE_APP;  // not supported on Android
  Profile* profile = GetProfile();

  BrowserWindowInterface* new_browser_window =
      CreateBrowserWindowAsync(type, profile);

  EXPECT_EQ(new_browser_window, nullptr);
}

IN_PROC_BROWSER_TEST_F(
    CreateBrowserWindowAndroidBrowserTest,
    CreateBrowserWindowAsync_UnsupportedInitialShowState_TriggersCallbackWithNull) {
  auto type = BrowserWindowInterface::Type::TYPE_NORMAL;
  Profile* profile = GetProfile();
  auto initial_show_state =
      ui::mojom::WindowShowState::kFullscreen;  // not supported on Android

  BrowserWindowInterface* new_browser_window =
      CreateBrowserWindowAsync(type, profile, initial_show_state);

  EXPECT_EQ(new_browser_window, nullptr);
}

IN_PROC_BROWSER_TEST_F(CreateBrowserWindowAndroidBrowserTest,
                       CreateBrowserWindowSync_IncognitoDisabled_ReturnsNull) {
  Profile* incognito_profile =
      GetProfile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  GetProfile()->GetPrefs()->SetInteger(
      policy::policy_prefs::kIncognitoModeAvailability,
      static_cast<int>(policy::IncognitoModeAvailability::kDisabled));

  auto type = BrowserWindowInterface::Type::TYPE_NORMAL;

  BrowserWindowInterface* new_browser_window =
      CreateBrowserWindowSync(type, incognito_profile);

  EXPECT_EQ(new_browser_window, nullptr);
}

IN_PROC_BROWSER_TEST_F(
    CreateBrowserWindowAndroidBrowserTest,
    CreateBrowserWindowAsync_IncognitoDisabled_TriggersCallbackWithNull) {
  Profile* incognito_profile =
      GetProfile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  GetProfile()->GetPrefs()->SetInteger(
      policy::policy_prefs::kIncognitoModeAvailability,
      static_cast<int>(policy::IncognitoModeAvailability::kDisabled));

  auto type = BrowserWindowInterface::Type::TYPE_NORMAL;

  BrowserWindowInterface* new_browser_window =
      CreateBrowserWindowAsync(type, incognito_profile);

  EXPECT_EQ(new_browser_window, nullptr);
}

IN_PROC_BROWSER_TEST_F(
    CreateBrowserWindowAndroidBrowserTest,
    CreateBrowserWindowAsync_ReturnsBrowserWindowAsLastActivatedWindow) {
  auto type = BrowserWindowInterface::Type::TYPE_NORMAL;
  Profile* profile = GetProfile();

  // 1. Create a new browser window asynchronously.
  BrowserWindowInterface* new_browser_window =
      CreateBrowserWindowAsync(type, profile);

  // 2. Obtain the last active browser window.
  // This call would have crashed without the fix, as it sorts windows by their
  // activation time.
  BrowserWindowInterface* last_active_browser_window =
      GetLastActiveBrowserWindowInterfaceWithAnyProfile();

  // 3. Verify the newly created window is the last active window.
  EXPECT_EQ(new_browser_window, last_active_browser_window);
}

IN_PROC_BROWSER_TEST_F(
    CreateBrowserWindowAndroidBrowserTest,
    CreateBrowserWindowAsync_WithWebContents_TabHasWebContents) {
  auto type = BrowserWindowInterface::Type::TYPE_NORMAL;
  Profile* profile = GetProfile();

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(content::WebContents::CreateParams(profile));
  content::WebContents* expected_web_contents = web_contents.get();

  BrowserWindowCreateParams create_params(type, *profile, false);
  create_params.web_contents = std::move(web_contents);

  base::test::TestFuture<BrowserWindowInterface*> future;
  CreateBrowserWindow(std::move(create_params), future.GetCallback());
  BrowserWindowInterface* new_browser_window = future.Get();

  AssertBrowserWindow(new_browser_window, type, profile,
                      /*expect_fully_initialized=*/true);

  auto* tab_list_interface = TabListInterface::From(new_browser_window);
  ASSERT_NE(tab_list_interface, nullptr);
  auto* tab = tab_list_interface->GetActiveTab();
  EXPECT_EQ(tab_list_interface->GetTabCount(), 1);
  ASSERT_NE(tab, nullptr);
  EXPECT_EQ(tab->GetContents(), expected_web_contents);
}

IN_PROC_BROWSER_TEST_F(
    CreateBrowserWindowAndroidBrowserTest,
    CreateBrowserWindowAsync_PopupWithWebContents_TabHasWebContents) {
  auto type = BrowserWindowInterface::Type::TYPE_POPUP;
  Profile* profile = GetProfile();

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(content::WebContents::CreateParams(profile));
  content::WebContents* expected_web_contents = web_contents.get();

  NavigationStartedObserver observer(expected_web_contents);

  BrowserWindowCreateParams create_params(type, *profile, false);
  create_params.web_contents = std::move(web_contents);

  base::test::TestFuture<BrowserWindowInterface*> future;
  CreateBrowserWindow(std::move(create_params), future.GetCallback());
  BrowserWindowInterface* new_browser_window = future.Get();

  AssertBrowserWindow(new_browser_window, type, profile,
                      /*expect_fully_initialized=*/true);

  auto* tab_list_interface = TabListInterface::From(new_browser_window);
  ASSERT_NE(tab_list_interface, nullptr);
  auto* tab = tab_list_interface->GetActiveTab();
  ASSERT_NE(tab, nullptr);
  EXPECT_EQ(tab->GetContents(), expected_web_contents);
  EXPECT_FALSE(observer.navigation_started());
}
