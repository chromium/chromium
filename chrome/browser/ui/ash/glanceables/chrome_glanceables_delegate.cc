// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/glanceables/chrome_glanceables_delegate.h"

#include "ash/glanceables/glanceables_controller.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/chrome_switches.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace {
ChromeGlanceablesDelegate* g_instance = nullptr;
}  // namespace

ChromeGlanceablesDelegate::ChromeGlanceablesDelegate(
    ash::GlanceablesController* controller)
    : controller_(controller) {
  DCHECK(controller_);

  DCHECK(!g_instance);
  g_instance = this;
}

ChromeGlanceablesDelegate::~ChromeGlanceablesDelegate() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
ChromeGlanceablesDelegate* ChromeGlanceablesDelegate::Get() {
  return g_instance;
}

void ChromeGlanceablesDelegate::OnPrimaryUserSessionStarted(Profile* profile) {
  identity_manager_ = IdentityManagerFactory::GetForProfileIfExists(profile);
  DCHECK(identity_manager_);

  if (!ShouldShowOnLogin())
    return;

  // User session is marked as started after the login screen is dismissed, and
  // session state change and auth state checking happens in parallel. It's not
  // guaranteed that the refresh token (which is needed for the calendar
  // service) is available at this moment.
  bool has_refresh_token_for_primary_account =
      identity_manager_->HasPrimaryAccountWithRefreshToken(
          signin::ConsentLevel::kSignin);
  if (has_refresh_token_for_primary_account)
    controller_->ShowOnLogin();
  else
    identity_manager_->AddObserver(this);
}

void ChromeGlanceablesDelegate::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  const CoreAccountInfo& primary_account_info =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  if (account_info != primary_account_info)
    return;

  identity_manager_->RemoveObserver(this);
  controller_->ShowOnLogin();
}

bool ChromeGlanceablesDelegate::ShouldShowOnLogin() const {
  // Skip glanceables when --no-first-run is passed. This prevents glanceables
  // from interfering with existing browser tests (they pass this switch) and is
  // also helpful when bisecting.
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kNoFirstRun);
}
