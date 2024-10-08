// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/mahi/public/cpp/mahi_manager.h"

#include "base/check_is_test.h"
#include "base/check_op.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_switches.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/gaia/gaia_auth_util.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/startup/browser_params_proxy.h"
#endif

namespace chromeos {

namespace {

MahiManager* g_instance = nullptr;
MahiManager* g_manager_instance_for_testing = nullptr;

}  // namespace

// MahiOutline ----------------------------------------------------------------

bool MahiOutline::operator==(const MahiOutline&) const = default;

// MahiManager -----------------------------------------------------------------

// static
// TODO(b:356035887): this may return different ptrs to callers depending on
// timing in test. Minimize the use of this global getter, and get rid of
// overriding global instance while there exists a live one.
MahiManager* MahiManager::Get() {
  if (g_manager_instance_for_testing) {
    return g_manager_instance_for_testing;
  }
  return g_instance;
}

MahiManager::MahiManager() {
  if (g_instance) {
    CHECK_IS_TEST();
  } else {
    g_instance = this;
  }
}

MahiManager::~MahiManager() {
  if (g_instance != this) {
    CHECK_IS_TEST();
  } else {
    g_instance = nullptr;
  }
}

std::optional<base::UnguessableToken> MahiManager::GetMediaAppPDFClientId()
    const {
  return std::nullopt;
}

// static
ScopedMahiManagerSetter* ScopedMahiManagerSetter::instance_ = nullptr;

ScopedMahiManagerSetter::ScopedMahiManagerSetter(MahiManager* manager) {
  // Only allow one scoped instance at a time.
  if (instance_) {
    NOTREACHED_IN_MIGRATION();
    return;
  }
  instance_ = this;

  g_manager_instance_for_testing = manager;
}

ScopedMahiManagerSetter::~ScopedMahiManagerSetter() {
  if (instance_ != this) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  instance_ = nullptr;

  g_manager_instance_for_testing = nullptr;
}

}  // namespace chromeos
