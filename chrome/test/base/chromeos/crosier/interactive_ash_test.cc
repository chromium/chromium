// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chromeos/crosier/interactive_ash_test.h"

#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/app_restore/full_restore_app_launch_handler.h"
#include "chrome/browser/ash/dbus/ash_dbus_helper.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "url/gurl.h"

namespace {

#if BUILDFLAG(IS_CHROMEOS_DEVICE)
class FakeSessionManagerClientBrowserHelper
    : public ash::DBusHelperObserverForTest {
 public:
  FakeSessionManagerClientBrowserHelper() {
    ash::DBusHelperObserverForTest::Set(this);
  }
  FakeSessionManagerClientBrowserHelper(
      const FakeSessionManagerClientBrowserHelper&) = delete;
  FakeSessionManagerClientBrowserHelper& operator=(
      const FakeSessionManagerClientBrowserHelper&) = delete;
  ~FakeSessionManagerClientBrowserHelper() override {
    ash::DBusHelperObserverForTest::Set(nullptr);
  }

  // ash::DBusHelperObserverForTest:
  void PostInitializeDBus() override {
    // Create FakeSessionManageClient after real SessionManagerClient is created
    // and before it is referenced.
    scoped_fake_session_manager_client_.emplace();
    ash::FakeSessionManagerClient::Get()->set_stop_session_callback(
        base::BindOnce(&chrome::ExitIgnoreUnloadHandlers));
  }

  void PreShutdownDBus() override {
    // Release FakeSessionManagerClient shutting down dbus clients.
    scoped_fake_session_manager_client_.reset();
  }

 private:
  // Optionally, use FakeSessionManagerClient if a test only needs the stub
  // user session.
  absl::optional<ash::ScopedFakeSessionManagerClient>
      scoped_fake_session_manager_client_;
};
#endif

}  // namespace

using InteractiveMixinBasedBrowserTest =
    InteractiveBrowserTestT<MixinBasedInProcessBrowserTest>;

InteractiveAshTest::InteractiveAshTest() {
  // See header file class comment.
  set_launch_browser_for_testing(nullptr);

  // Give all widgets the same Kombucha context.This is useful for ash system
  // UI because the UI uses a variety of small widgets. Note that if this test
  // used multiple displays we would need to provide a different context per
  // display (i.e. the widget's native window's root window). Elements like
  // the home button, shelf, etc. appear once per display.
  views::ElementTrackerViews::SetContextOverrideCallback(
      base::BindRepeating([](views::Widget* widget) {
        return ui::ElementContext(ash::Shell::GetPrimaryRootWindow());
      }));

#if BUILDFLAG(IS_CHROMEOS_DEVICE)
  if (!use_real_session_manager_) {
    fake_session_manager_client_helper_ =
        std::make_unique<FakeSessionManagerClientBrowserHelper>();
  }
#endif
}

InteractiveAshTest::~InteractiveAshTest() {
  views::ElementTrackerViews::SetContextOverrideCallback({});
}

void InteractiveAshTest::SetupContextWidget() {
  views::Widget* status_area_widget =
      ash::Shell::GetPrimaryRootWindowController()
          ->shelf()
          ->GetStatusAreaWidget();
  SetContextWidget(status_area_widget);
}

void InteractiveAshTest::InstallSystemApps() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  CHECK(profile);
  ash::SystemWebAppManager::GetForTest(profile)->InstallSystemAppsForTesting();
}

Profile* InteractiveAshTest::GetActiveUserProfile() {
  return ProfileManager::GetActiveUserProfile();
}

base::WeakPtr<content::NavigationHandle>
InteractiveAshTest::CreateBrowserWindow(const GURL& url) {
  Profile* profile = GetActiveUserProfile();
  CHECK(profile);
  NavigateParams params(profile, url, ui::PAGE_TRANSITION_TYPED);
  params.disposition = WindowOpenDisposition::NEW_WINDOW;
  params.window_action = NavigateParams::SHOW_WINDOW;
  return Navigate(&params);
}
