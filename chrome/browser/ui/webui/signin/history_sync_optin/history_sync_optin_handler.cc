// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/history_sync_optin/history_sync_optin_handler.h"

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"

namespace {
history_sync_optin::mojom::AccountInfoPtr CreateAccountInfoDataMojo(
    const AccountInfo& info) {
  history_sync_optin::mojom::AccountInfoPtr account_info_mojo =
      history_sync_optin::mojom::AccountInfo::New();
  account_info_mojo->account_image_src =
      GURL(signin::GetAccountPictureUrl(info));
  return account_info_mojo;
}
}  // namespace

HistorySyncOptinHandler::HistorySyncOptinHandler(
    mojo::PendingReceiver<history_sync_optin::mojom::PageHandler> receiver,
    mojo::PendingRemote<history_sync_optin::mojom::Page> page,
    Browser* browser,
    Profile* profile)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      browser_(browser ? browser->AsWeakPtr() : nullptr),
      profile_(profile),
      identity_manager_(IdentityManagerFactory::GetForProfile(profile_)) {
  CHECK(profile_);
  CHECK(identity_manager_);
}

HistorySyncOptinHandler::~HistorySyncOptinHandler() = default;

void HistorySyncOptinHandler::Accept() {
  AddHistorySyncConsent();
  FinishAndCloseDialog();
}

void HistorySyncOptinHandler::Reject() {
  FinishAndCloseDialog();
}

void HistorySyncOptinHandler::RequestAccountInfo() {
  MaybeGetAccountInfo();
}

void HistorySyncOptinHandler::MaybeGetAccountInfo() {
  AccountInfo primary_account_info = identity_manager_->FindExtendedAccountInfo(
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin));

  if (!primary_account_info.IsEmpty()) {
    DispatchAccountInfoUpdate(primary_account_info);
  }

  if (!identity_manager_observation_.IsObserving()) {
    identity_manager_observation_.Observe(identity_manager_);
  }
}

void HistorySyncOptinHandler::UpdateDialogHeight(uint32_t height) {
  if (browser_) {
    browser_->signin_view_controller()->SetModalSigninHeight(height);
  }
}

void HistorySyncOptinHandler::FinishAndCloseDialog() {
  // TODO(crbug.com/404806506): Add metrics.
  if (browser_) {
    browser_->signin_view_controller()->CloseModalSignin();
  }
}

void HistorySyncOptinHandler::AddHistorySyncConsent() {
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile_);
  CHECK(sync_service);
  CHECK(identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  // TODO(crbug.com/404806988): As we add the invocation points check if additional actions
  // are needed to enable sync for history.
  // The invocation below works for an already syncing user. It enables the syncing for history
  // if it's not already turned on.
  sync_service->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kHistory, /*is_type_on=*/true);
}

void HistorySyncOptinHandler::OnAvatarChanged(const AccountInfo& info) {
  CHECK(info.IsValid());
  page_->SendAccountInfo(CreateAccountInfoDataMojo(info));
}

void HistorySyncOptinHandler::DispatchAccountInfoUpdate(
    const AccountInfo& info) {
  if (info.IsEmpty()) {
    // No account is signed in, so there is nothing to be displayed in the sync
    // confirmation dialog.
    return;
  }
  if (info.account_id !=
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin)) {
    return;
  }
  if (info.IsValid()) {
    OnAvatarChanged(info);
  }
}

void HistorySyncOptinHandler::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  DispatchAccountInfoUpdate(info);
}
