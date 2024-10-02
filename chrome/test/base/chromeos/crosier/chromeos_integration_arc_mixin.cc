// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chromeos/crosier/chromeos_integration_arc_mixin.h"

#include "ash/components/arc/metrics/arc_metrics_constants.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/test/active_window_waiter.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/arc/boot_phase_monitor/arc_boot_phase_monitor_bridge.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/chromeos/crosier/chromeos_integration_login_mixin.h"
#include "chrome/test/base/chromeos/crosier/helper/test_sudo_helper_client.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_context.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/events/event_constants.h"

namespace {

// ArcBootWaiter waits for boot completed from `ArcBootPhaseMonitorBridge`.
class ArcBootWaiter : public arc::ArcBootPhaseMonitorBridge::Observer {
 public:
  ArcBootWaiter() = default;
  ~ArcBootWaiter() override = default;

  void Wait() {
    const user_manager::User* primary_user =
        user_manager::UserManager::Get()->GetPrimaryUser();
    CHECK(primary_user);

    auto* browser_context =
        ash::BrowserContextHelper::Get()->GetBrowserContextByUser(primary_user);
    arc::ArcBootPhaseMonitorBridge* boot_bridge =
        arc::ArcBootPhaseMonitorBridge::GetForBrowserContext(browser_context);
    CHECK(boot_bridge);

    scoped_observation_.Observe(boot_bridge);

    wait_loop_.Run();
  }

  // arc::ArcBootPhaseMonitorBridge::Observer:
  void OnBootCompleted() override { wait_loop_.Quit(); }

 private:
  base::RunLoop wait_loop_;
  base::ScopedObservation<arc::ArcBootPhaseMonitorBridge,
                          arc::ArcBootPhaseMonitorBridge::Observer>
      scoped_observation_{this};
};

// AppReadyWaiter waits until the given `app_id` is ready and launchable.
class AppReadyWaiter : public ArcAppListPrefs::Observer {
 public:
  AppReadyWaiter(ArcAppListPrefs* arc_app_list_prefs, std::string_view app_id)
      : prefs_(arc_app_list_prefs), app_id_(app_id) {
    scoped_observation_.Observe(arc_app_list_prefs);
  }

  void Wait() {
    if (IsAppReadyAndLaunchable()) {
      return;
    }

    wait_loop_.Run();

    CHECK(IsAppReadyAndLaunchable());
  }

  // ArcAppListPrefs::Observer:
  void OnAppRegistered(const std::string& app_id,
                       const ArcAppListPrefs::AppInfo& app_info) override {
    if (app_id == app_id_ && IsAppReadyAndLaunchable()) {
      wait_loop_.Quit();
    }
  }
  void OnAppStatesChanged(const std::string& app_id,
                          const ArcAppListPrefs::AppInfo& app_info) override {
    if (app_id == app_id_ && IsAppReadyAndLaunchable()) {
      wait_loop_.Quit();
    }
  }

 private:
  bool IsAppReadyAndLaunchable() const {
    auto app_info = prefs_->GetApp(app_id_);
    if (!app_info) {
      return false;
    }

    return app_info->ready && app_info->launchable;
  }

  const raw_ptr<ArcAppListPrefs> prefs_;
  const std::string app_id_;
  base::ScopedObservation<ArcAppListPrefs, ArcAppListPrefs::Observer>
      scoped_observation_{this};
  base::RunLoop wait_loop_;
};

// WindowAppIdWaiter waits for `ash::kAppIDKey` property set on a given window.
class WindowAppIdWaiter : public aura::WindowObserver {
 public:
  explicit WindowAppIdWaiter(aura::Window* window) {
    observation_.Observe(window);
  }

  const std::string* Wait() {
    found_app_id_ = observation_.GetSource()->GetProperty(ash::kAppIDKey);
    if (!found_app_id_) {
      run_loop_.Run();
    }

    return found_app_id_.get();
  }

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override {
    if (key != ash::kAppIDKey) {
      return;
    }

    found_app_id_ = window->GetProperty(ash::kAppIDKey);
    run_loop_.Quit();
  }
  void OnWindowDestroyed(aura::Window* window) override {
    observation_.Reset();
    run_loop_.Quit();
  }

 private:
  base::RunLoop run_loop_;
  raw_ptr<std::string> found_app_id_ = nullptr;
  base::ScopedObservation<aura::Window, aura::WindowObserver> observation_{
      this};
};

// Returns whether ARCVM should be used based on tast_use_flags.txt.
bool ShouldEnableArcVm() {
  base::ScopedAllowBlockingForTesting allow_blocking;
  std::string use_flags;
  CHECK(base::ReadFileToString(
      base::FilePath("/usr/local/etc/tast_use_flags.txt"), &use_flags));
  return base::Contains(use_flags, "arcvm") &&
         !base::Contains(use_flags, "arcpp");
}

// Gets the active user's browser context.
content::BrowserContext* GetActiveUserBrowserContext() {
  auto* user = user_manager::UserManager::Get()->GetActiveUser();
  return ash::BrowserContextHelper::Get()->GetBrowserContextByUser(user);
}

void WaitForAppRegister(const std::string& app_id) {
  AppReadyWaiter(ArcAppListPrefs::Get(GetActiveUserBrowserContext()), app_id)
      .Wait();
}

}  // namespace

ChromeOSIntegrationArcMixin::ChromeOSIntegrationArcMixin(
    InProcessBrowserTestMixinHost* host,
    const ChromeOSIntegrationLoginMixin& login_mixin)
    : InProcessBrowserTestMixin(host), login_mixin_(login_mixin) {}

ChromeOSIntegrationArcMixin::~ChromeOSIntegrationArcMixin() = default;

void ChromeOSIntegrationArcMixin::SetMode(Mode mode) {
  CHECK(!setup_called_);
  mode_ = mode;
}

void ChromeOSIntegrationArcMixin::SetUp() {
  setup_called_ = true;
}

void ChromeOSIntegrationArcMixin::WaitForBootAndConnectAdb() {
  ArcBootWaiter().Wait();
  adb_helper_.Intialize();

  const bool needs_play_store = (mode_ == Mode::kSupported);
  if (!needs_play_store) {
    // Disable play store. Otherwise it crashes.
    CHECK(adb_helper_.Command(
        "shell pm disable-user --user 0 com.android.vending"));
  }
}

bool ChromeOSIntegrationArcMixin::InstallApk(const base::FilePath& apk_path) {
  return adb_helper_.InstallApk(apk_path);
}

aura::Window* ChromeOSIntegrationArcMixin::LaunchAndWaitForWindow(
    const std::string& package,
    const std::string& activity) {
  const std::string app_id = ArcAppListPrefs::GetAppId(package, activity);
  WaitForAppRegister(app_id);

  // Launch the given activity.
  CHECK(arc::LaunchApp(GetActiveUserBrowserContext(), app_id, ui::EF_NONE,
                       arc::UserInteractionType::NOT_USER_INITIATED));

  // Wait for the activity window to be activated.
  aura::Window* const window =
      ash::ActiveWindowWaiter(ash::Shell::GetPrimaryRootWindow()).Wait();
  const std::string* window_app_id = WindowAppIdWaiter(window).Wait();
  CHECK(window_app_id);
  CHECK(!window_app_id->empty());
  CHECK_EQ(*window_app_id, app_id);
  return window;
}

void ChromeOSIntegrationArcMixin::SetUpCommandLine(
    base::CommandLine* command_line) {
  if (mode_ == Mode::kNone) {
    command_line->AppendSwitchASCII(ash::switches::kArcAvailability, "none");
    return;
  }

  CHECK(login_mixin_.mode() != ChromeOSIntegrationLoginMixin::Mode::kStubLogin)
      << "ARC does not work with stub login.";

  // User data dir needs to be "/home/chronos". Otherwise,
  // `IsArcCompatibleFileSystemUsedForUser()` returns false and ARC could not be
  // enabled.
  command_line->AppendSwitchASCII(::switches::kUserDataDir, "/home/chronos");

  if (ShouldEnableArcVm()) {
    command_line->AppendSwitch(ash::switches::kEnableArcVm);
  }

  // Common setup for both "Enabled" and "Supported" modes. The switches here
  // are from "tast-tests/cros/local/chrome/internal/setup/restart.go".
  // Reference: http://shortn/_P4IIm7c7aY
  command_line->AppendSwitch(ash::switches::kArcDisableAppSync);
  command_line->AppendSwitch(ash::switches::kArcDisablePlayAutoInstall);
  command_line->AppendSwitch(ash::switches::kArcDisableLocaleSync);
  command_line->AppendSwitchASCII(ash::switches::kArcPlayStoreAutoUpdate,
                                  "off");
  command_line->AppendSwitch(ash::switches::kArcDisableMediaStoreMaintenance);
  command_line->AppendSwitch(ash::switches::kDisableArcCpuRestriction);

  if (mode_ == Mode::kEnabled) {
    command_line->AppendSwitch(ash::switches::kDisableArcOptInVerification);
    command_line->AppendSwitchASCII(ash::switches::kArcStartMode,
                                    "always-start-with-no-play-store");

    // The "installed" mode needs `kEnableArcFeature` to work.
    // See "IsArcAvailable()" in ash/components/arc/arc_util.cc.
    command_line->AppendSwitchASCII(ash::switches::kArcAvailability,
                                    "installed");
    scoped_feature_list_.emplace();
    scoped_feature_list_->InitFromCommandLine("EnableARC", base::EmptyString());
  }

  if (mode_ == Mode::kSupported) {
    command_line->AppendSwitchASCII(ash::switches::kArcAvailability,
                                    "officially-supported");
  }
}
