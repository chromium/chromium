// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_ARC_UTIL_H_
#define COMPONENTS_ARC_ARC_UTIL_H_

// This file contains utility to see ARC functionality status controlled by
// outside of ARC, e.g. CommandLine flag, attribute of global data/state,
// users' preferences, and FeatureList.

#include <stdint.h>
#include <string>

namespace aura {
class Window;
}  // namespace aura

namespace base {
class CommandLine;
}  // namespace base

namespace user_manager {
class User;
}  // namespace user_manager

namespace arc {

// Name of the crosvm instance when ARCVM is enabled.
constexpr char kArcVmName[] = "arcvm";

// Returns true if ARC is installed and the current device is officially
// supported to run ARC.
// Note that, to run ARC practically, it is necessary to meet more conditions,
// e.g., ARC supports only on Primary User Profile. To see if ARC can actually
// run for the profile etc., arc::ArcSessionManager::IsAllowedForProfile() is
// the function for that purpose. Please see also its comment, too.
// Also note that, ARC singleton classes (e.g. ArcSessionManager,
// ArcServiceManager, ArcServiceLauncher) are instantiated regardless of this
// check, so it is ok to access them directly.
bool IsArcAvailable();

// Returns true if ARC VM is enabled.
bool IsArcVmEnabled();

// Returns true if ARC should always start within the primary user session
// (opted in user or not), and other supported mode such as guest and Kiosk
// mode.
bool ShouldArcAlwaysStart();

// Returns true if ARC should always start with no Play Store availability
// within the primary user session (opted in user or not), and other supported
// mode such as guest and Kiosk mode.
bool ShouldArcAlwaysStartWithNoPlayStore();

// Returns true if ARC OptIn ui needs to be shown for testing.
bool ShouldShowOptInForTesting();

// Enables to always start ARC without Play Store for testing, by appending the
// command line flag.
void SetArcAlwaysStartWithoutPlayStoreForTesting();

// Returns true if ARC is installed and running ARC kiosk apps on the current
// device is officially supported.
// It doesn't follow that ARC is available for user sessions and
// IsArcAvailable() will return true, although the reverse should be.
// This is used to distinguish special cases when ARC kiosk is available on
// the device, but is not yet supported for regular user sessions.
// In most cases, IsArcAvailable() check should be used instead of this.
// Also not that this function may return true when ARC is not running in
// Kiosk mode, it checks only ARC Kiosk availability.
bool IsArcKioskAvailable();

// For testing ARC in browser tests, this function should be called in
// SetUpCommandLine(), and its argument should be passed to this function.
// Also, in unittests, this can be called in SetUp() with
// base::CommandLine::ForCurrentProcess().
// |command_line| must not be nullptr.
void SetArcAvailableCommandLineForTesting(base::CommandLine* command_line);

// Returns true if ARC should run under Kiosk mode for the current profile.
// As it can return true only when user is already initialized, it implies
// that ARC availability was checked before and IsArcKioskAvailable()
// should also return true in that case.
bool IsArcKioskMode();

// Returns true if current user is a robot account user, or offline demo mode
// user.
// These are Public Session and ARC Kiosk users. Note that demo mode, including
// offline demo mode, is implemented as a Public Session - offline demo mode
// is setup offline and it isn't associated with a working robot account.
// As it can return true only when user is already initialized, it implies
// that ARC availability was checked before.
// The check is basically IsArcKioskMode() | IsLoggedInAsPublicSession().
bool IsRobotOrOfflineDemoAccountMode();

// Returns true if ARC is allowed for the given user. Note this should not be
// used as a signal of whether ARC is allowed alone because it only considers
// user meta data. e.g. a user could be allowed for ARC but if the user signs in
// as a secondary user or signs in to create a supervised user, ARC should be
// disabled for such cases.
bool IsArcAllowedForUser(const user_manager::User* user);

// Checks if opt-in verification was disabled by switch in command line.
// In most cases, it is disabled for testing purpose.
bool IsArcOptInVerificationDisabled();

// Returns true if the |window|'s aura::client::kAppType is ARC_APP. When
// |window| is nullptr, returns false.
bool IsArcAppWindow(const aura::Window* window);

constexpr int kNoTaskId = -1;
constexpr int kSystemWindowTaskId = 0;
// Returns the task id given by the exo shell's application id, or |kNoTaskId|
// if not an ARC window.
int GetWindowTaskId(const aura::Window* window);
int GetTaskIdFromWindowAppId(const std::string& app_id);

// Returns true if ARC app icons are forced to cache.
bool IsArcForceCacheAppIcon();

// Returns true if data clean up is requested for each ARC start.
bool IsArcDataCleanupOnStartRequested();

// Returns true in case ARC app sync flow is disabled.
bool IsArcAppSyncFlowDisabled();

// Returns true in case ARC locale sync is disabled.
bool IsArcLocaleSyncDisabled();

// Returns true in case ARC Play Auto Install flow is disabled.
bool IsArcPlayAutoInstallDisabled();

// Adjusts the amount of CPU the ARC instance is allowed to use. When
// |do_restrict| is true, the limit is adjusted so ARC can only use tightly
// restricted CPU resources.
// TODO(yusukes): Use enum instead of bool.
void SetArcCpuRestriction(bool do_restrict);

// Returns the Android density that should be used for the given device scale
// factor used on chrome.
int32_t GetLcdDensityForDeviceScaleFactor(float device_scale_factor);

}  // namespace arc

#endif  // COMPONENTS_ARC_ARC_UTIL_H_
