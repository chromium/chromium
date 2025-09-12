// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/history_sync_optin_service.h"

#include "base/notreached.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"

HistorySyncOptinServiceDefaultDelegate::
    HistorySyncOptinServiceDefaultDelegate() = default;

HistorySyncOptinServiceDefaultDelegate::
    ~HistorySyncOptinServiceDefaultDelegate() = default;

void HistorySyncOptinServiceDefaultDelegate::ShowHistorySyncOptinScreen(
    Profile* profile,
    base::OnceClosure history_optin_completed_closure) {
  CHECK(profile);
  Browser* browser = chrome::FindLastActiveWithProfile(profile);
  CHECK(browser);
  browser->GetFeatures()
      .signin_view_controller()
      ->ShowModalHistorySyncOptInDialog(
          std::move(history_optin_completed_closure));
}

void HistorySyncOptinServiceDefaultDelegate::ShowAccountManagementScreen(
    signin::SigninChoiceCallback on_account_management_screen_closed) {
  // Flows with access to a browser should rely on
  // `ProfileManagementDisclaimerService` for displaying management screens.
  NOTREACHED();
}

void HistorySyncOptinServiceDefaultDelegate::
    FinishFlowWithoutHistorySyncOptin() {}

HistorySyncOptinService::HistorySyncOptinService(Profile* profile)
    : profile_(profile) {}

HistorySyncOptinService::~HistorySyncOptinService() = default;

bool HistorySyncOptinService::StartHistorySyncOptinFlow(
    const AccountInfo& account_info,
    std::unique_ptr<HistorySyncOptinHelper::Delegate> delegate) {
  if (history_sync_optin_helper_) {
    // Another flow is already in progress, abort the new flow.
    return false;
  }
  history_sync_optin_delegate_ = std::move(delegate);

  CHECK(history_sync_optin_delegate_);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  history_sync_optin_helper_ = HistorySyncOptinHelper::Create(
      identity_manager, profile_, account_info,
      history_sync_optin_delegate_.get(),
      HistorySyncOptinHelper::LaunchContext::kInBrowser);
  history_sync_optin_observation_.Observe(history_sync_optin_helper_.get());
  history_sync_optin_helper_->StartHistorySyncOptinFlow();
  return true;
}

void HistorySyncOptinService::Shutdown() {
  history_sync_optin_observation_.Reset();
  history_sync_optin_helper_.reset();
  history_sync_optin_delegate_.reset();
}

void HistorySyncOptinService::OnHistorySyncOptinHelperFlowFinished() {
  Shutdown();
}
