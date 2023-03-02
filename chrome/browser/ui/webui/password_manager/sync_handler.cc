// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/password_manager/sync_handler.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_service_utils.h"
#include "components/sync/driver/sync_user_settings.h"

namespace password_manager {

SyncHandler::SyncHandler(Profile* profile) : profile_(profile) {}

SyncHandler::~SyncHandler() = default;

void SyncHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "GetSyncTrustedVaultBannerState",
      base::BindRepeating(&SyncHandler::HandleGetTrustedVaultBannerState,
                          base::Unretained(this)));
}

void SyncHandler::OnJavascriptAllowed() {
  // This is intentionally not using GetSyncService(), to go around the
  // Profile::IsSyncAllowed() check.
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile_);
  if (sync_service) {
    sync_service_observation_.Observe(sync_service);
  }
}

void SyncHandler::OnJavascriptDisallowed() {
  sync_service_observation_.Reset();
}

base::Value SyncHandler::GetTrustedVaultBannerState() const {
  syncer::SyncService* sync_service = GetSyncService();
  auto state = TrustedVaultBannerState::kNotShown;
  if (sync_service && sync_service->GetUserSettings()->GetPassphraseType() ==
                          syncer::PassphraseType::kTrustedVaultPassphrase) {
    state = TrustedVaultBannerState::kOptedIn;
  } else if (syncer::ShouldOfferTrustedVaultOptIn(sync_service)) {
    state = TrustedVaultBannerState::kOfferOptIn;
  }

  return base::Value(static_cast<int>(state));
}

void SyncHandler::HandleGetTrustedVaultBannerState(
    const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];

  ResolveJavascriptCallback(callback_id, GetTrustedVaultBannerState());
}

void SyncHandler::OnStateChanged(syncer::SyncService* sync_service) {
  FireWebUIListener("trusted-vault-banner-state-changed",
                    GetTrustedVaultBannerState());
}

syncer::SyncService* SyncHandler::GetSyncService() const {
  return SyncServiceFactory::IsSyncAllowed(profile_)
             ? SyncServiceFactory::GetForProfile(profile_)
             : nullptr;
}

}  // namespace password_manager
