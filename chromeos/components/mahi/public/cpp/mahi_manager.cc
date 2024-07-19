// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/mahi/public/cpp/mahi_manager.h"

#include "base/check_op.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chromeos/constants/chromeos_features.h"

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

}  // namespace

// MahiOutline ----------------------------------------------------------------

bool MahiOutline::operator==(const MahiOutline&) const = default;

// MahiManager -----------------------------------------------------------------

// static
MahiManager* MahiManager::Get() {
  return g_instance;
}

MahiManager::MahiManager() {
  DCHECK(!g_instance);
  g_instance = this;
}

MahiManager::~MahiManager() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
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

  // Save the real manager instance and replace it with the fake one.
  real_manager_instance_ = g_instance;
  g_instance = manager;
}

ScopedMahiManagerSetter::~ScopedMahiManagerSetter() {
  if (instance_ != this) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  instance_ = nullptr;

  g_instance = real_manager_instance_;
  real_manager_instance_ = nullptr;
}

}  // namespace chromeos
