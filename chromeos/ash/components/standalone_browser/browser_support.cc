// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/standalone_browser/browser_support.h"

#include "ash/constants/ash_switches.h"
#include "base/check_is_test.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/debug/dump_without_crashing.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/system/sys_info.h"
#include "chromeos/ash/components/standalone_browser/lacros_availability.h"
#include "chromeos/ash/components/standalone_browser/migrator_util.h"
#include "chromeos/ash/components/standalone_browser/standalone_browser_features.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"

namespace ash::standalone_browser {
namespace {

BrowserSupport* g_instance = nullptr;
std::optional<bool> g_cpu_supported_override_ = std::nullopt;

// Returns true if `kDisallowLacros` is set by command line.
bool IsLacrosDisallowedByCommand() {
  const base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  return cmdline->HasSwitch(ash::switches::kDisallowLacros) &&
         !cmdline->HasSwitch(ash::switches::kDisableDisallowLacros);
}

// Some account types require features that aren't yet supported by lacros.
// See https://crbug.com/1080693
bool IsUserTypeAllowed(const user_manager::User& user) {
  switch (user.GetType()) {
    case user_manager::UserType::kRegular:
    case user_manager::UserType::kPublicAccount:
    // Note: Lacros will not be enabled for Guest users unless LacrosOnly
    // flag is passed in --enable-features. See https://crbug.com/1294051#c25.
    case user_manager::UserType::kGuest:
      return true;
    case user_manager::UserType::kChild:
      return base::FeatureList::IsEnabled(features::kLacrosForSupervisedUsers);
    case user_manager::UserType::kWebKioskApp:
      return base::FeatureList::IsEnabled(features::kWebKioskEnableLacros);
    case user_manager::UserType::kKioskApp:
      return base::FeatureList::IsEnabled(features::kChromeKioskEnableLacros);
    case user_manager::UserType::kArcKioskApp:
      return false;
  }
}

// Returns whether or not lacros is allowed for the Primary user,
// with given LacrosAvailability policy.
bool IsAllowedInternal(const user_manager::User* user,
                       LacrosAvailability lacros_availability) {
  if (IsLacrosDisallowedByCommand() || !BrowserSupport::IsCpuSupported()) {
    // This happens when Ash is restarted in multi-user session, meaning there
    // are more than two users logged in to the device. This will not cause an
    // accidental removal of Lacros data because for the primary user, the fact
    // that the device is in multi-user session means that Lacros was not
    // enabled beforehand. And for secondary users, data removal does not happen
    // even if Lacros is disabled.
    return false;
  }

  if (!user) {
    // User is not available. Practically, this is accidentally happening
    // if related function is called before session, or in testing.
    // TODO(crbug.com/1408962): We should limit this at least only for
    // testing.
    return false;
  }

  if (!IsUserTypeAllowed(*user)) {
    return false;
  }

  switch (lacros_availability) {
    case LacrosAvailability::kLacrosDisallowed:
      return false;
    case LacrosAvailability::kUserChoice:
    case LacrosAvailability::kLacrosOnly:
      return true;
  }
}

// Returns whether the standalone browser (a.k.a. Lacros) is enabled.
// Practically, before invoking this, BrowserSupport::IsAllowedInternal()
// needs to be called and make sure it returns true.
bool IsEnabledInternal(const user_manager::User* user,
                       LacrosAvailability lacros_availability,
                       bool check_migration_status) {
  DCHECK(user);

  // If profile migration is enabled, the completion of it is necessary for
  // Lacros to be enabled.
  if (check_migration_status &&
      !base::FeatureList::IsEnabled(
          features::kLacrosProfileMigrationForceOff)) {
    PrefService* local_state =
        user_manager::UserManager::Get()->GetLocalState();
    // Note that local_state can be nullptr in tests.
    if (local_state && !migrator_util::IsProfileMigrationCompletedForUser(
                           local_state, user->username_hash())) {
      // If migration has not been completed, do not enable lacros.
      return false;
    }
  }

  switch (lacros_availability) {
    case LacrosAvailability::kUserChoice:
      break;
    case LacrosAvailability::kLacrosDisallowed:
      NOTREACHED();  // Guarded by IsLacrosAllowedInternal, called before.
      return false;
    case LacrosAvailability::kLacrosOnly:
      return true;
  }

  if (base::FeatureList::IsEnabled(features::kLacrosOnly)) {
    return true;
  }

  return false;
}

}  // namespace

BrowserSupport::BrowserSupport(bool is_allowed) : is_allowed_(is_allowed) {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

BrowserSupport::~BrowserSupport() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void BrowserSupport::InitializeForPrimaryUser(
    const policy::PolicyMap& policy_map) {
  // Currently, some tests rely on initializing ProfileManager a second time.
  // That causes this method to be called twice. Here, we take care of that
  // case by deallocating the old instance and allocating a new one.
  // TODO(andreaorru): remove the following code once there's no more tests
  // that rely on it.
  if (g_instance) {
    CHECK_IS_TEST();
    Shutdown();
  }

  auto* primary_user = user_manager::UserManager::Get()->GetPrimaryUser();
  CHECK(primary_user);

  auto lacros_availability = GetLacrosAvailability(primary_user, policy_map);
  auto is_allowed = IsAllowedInternal(primary_user, lacros_availability);

  // Calls the constructor, which in turn takes care of tracking the newly
  // created instance in `g_instance`, so that it's not leaked and can
  // later be destroyed via `Shutdown()`.
  new BrowserSupport(is_allowed);
}

// static
void BrowserSupport::Shutdown() {
  // Calls the destructor, which in turn takes care of setting `g_instance`
  // to NULL, to keep track of the state.
  delete g_instance;
}

bool BrowserSupport::IsInitializedForPrimaryUser() {
  return !!g_instance;
}

// static
BrowserSupport* BrowserSupport::GetForPrimaryUser() {
  DCHECK(g_instance);
  return g_instance;
}

// static
bool BrowserSupport::IsCpuSupported() {
  if (g_cpu_supported_override_.has_value()) {
    return *g_cpu_supported_override_;
  }

#ifdef ARCH_CPU_X86_64
  // Some very old Flex devices are not capable to support the SSE4.2
  // instruction set. Those CPUs should not use Lacros as Lacros has only one
  // binary for all x86-64 platforms.
  return __builtin_cpu_supports("sse4.2");
#else
  return true;
#endif
}

void BrowserSupport::SetCpuSupportedForTesting(std::optional<bool> value) {
  g_cpu_supported_override_ = value;
}

bool BrowserSupport::IsEnabledInternal(const user_manager::User* user,
                                       LacrosAvailability lacros_availability,
                                       bool check_migration_status) {
  if (!IsAllowedInternal(user, lacros_availability)) {
    return false;
  }

  return ash::standalone_browser::IsEnabledInternal(user, lacros_availability,
                                                    check_migration_status);
}

}  // namespace ash::standalone_browser
