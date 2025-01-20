// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/password_manager/sync_handler.h"

#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "components/password_manager/core/browser/features/password_manager_features_util.h"
#include "components/sync/base/data_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_utils.h"
#include "components/sync/service/sync_user_settings.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/webui/web_ui_util.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/profiles/batch_upload/batch_upload_service.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_service_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#endif

namespace password_manager {

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
namespace {

// Entry points to the Batch Upload dialog in the passwords settings section.
// It is a subset of the entry points in `BatchUploadService::EntryPoint`.
// WARNING: Keep synced with
// chrome/browser/resources/password_manager/sync_browser_proxy.ts.
enum class BatchUploadPasswordsEntryPoint {
  kPasswordManager = 0,
  kPromoCard = 1,

  kMaxValue = kPromoCard,
};

BatchUploadService::EntryPoint ToBatchUploadEntryPoint(
    BatchUploadPasswordsEntryPoint password_entry_point) {
  switch (password_entry_point) {
    case BatchUploadPasswordsEntryPoint::kPasswordManager:
      return BatchUploadService::EntryPoint::kPasswordManagerSettings;
    case BatchUploadPasswordsEntryPoint::kPromoCard:
      return BatchUploadService::EntryPoint::kPasswordPromoCard;
  }
}

}  // namespace
#endif

using password_manager::features_util::ShouldShowAccountStorageSettingToggle;

SyncHandler::SyncHandler(Profile* profile) : profile_(profile) {}

SyncHandler::~SyncHandler() = default;

void SyncHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "GetSyncTrustedVaultBannerState",
      base::BindRepeating(&SyncHandler::HandleGetTrustedVaultBannerState,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "GetAccountInfo", base::BindRepeating(&SyncHandler::HandleGetAccountInfo,
                                            base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "GetSyncInfo", base::BindRepeating(&SyncHandler::HandleGetSyncInfo,
                                         base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "GetLocalPasswordCount",
      base::BindRepeating(&SyncHandler::HandleGetLocalPasswordCount,
                          base::Unretained(this)));
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  web_ui()->RegisterMessageCallback(
      "OpenBatchUpload",
      base::BindRepeating(&SyncHandler::HandleOpenBatchUploadDialog,
                          base::Unretained(this)));
#endif
}

void SyncHandler::OnJavascriptAllowed() {
  // This is intentionally not using GetSyncService(), to go around the
  // Profile::IsSyncAllowed() check.
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile_);
  if (sync_service) {
    sync_service_observation_.Observe(sync_service);
  }

  signin::IdentityManager* identity_manager(
      IdentityManagerFactory::GetInstance()->GetForProfile(profile_));
  if (identity_manager) {
    identity_manager_observation_.Observe(identity_manager);
  }
}

void SyncHandler::OnJavascriptDisallowed() {
  sync_service_observation_.Reset();
  identity_manager_observation_.Reset();
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

base::Value::Dict SyncHandler::GetSyncInfo() const {
  base::Value::Dict dict;

  syncer::SyncService* sync_service = GetSyncService();
  // sync_service might be nullptr if SyncServiceFactory::IsSyncAllowed is
  // false.
  if (!sync_service) {
    return dict;
  }

  PrefService* pref_service = profile_->GetPrefs();
  syncer::UserSelectableTypeSet types =
      sync_service->GetUserSettings()->GetSelectedTypes();

  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile_);
  dict.Set("isEligibleForAccountStorage",
           (!identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync) &&
            ShouldShowAccountStorageSettingToggle(pref_service, sync_service)));
  dict.Set("isSyncingPasswords",
           (sync_service->IsSyncFeatureEnabled() &&
            types.Has(syncer::UserSelectableType::kPasswords)));
  return dict;
}

void SyncHandler::HandleGetSyncInfo(const base::Value::List& args) {
  AllowJavascript();

  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];

  ResolveJavascriptCallback(callback_id, GetSyncInfo());
}

base::Value::Dict SyncHandler::GetAccountInfo() const {
  signin::IdentityManager* identity_manager(
      IdentityManagerFactory::GetInstance()->GetForProfile(profile_));
  auto stored_account = identity_manager->FindExtendedAccountInfo(
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin));

  base::Value::Dict dict;
  dict.Set("email", stored_account.email);
  const auto& avatar_image = stored_account.account_image;
  if (!avatar_image.IsEmpty()) {
    dict.Set("avatarImage", webui::GetBitmapDataUrl(avatar_image.AsBitmap()));
  }
  return dict;
}

void SyncHandler::HandleGetAccountInfo(const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];

  ResolveJavascriptCallback(callback_id, GetAccountInfo());
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
void SyncHandler::HandleOpenBatchUploadDialog(const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  CHECK(args[0].is_int());
  int entry_point_int = args[0].GetInt();
  CHECK(entry_point_int >= 0 &&
        entry_point_int <=
            static_cast<int>(BatchUploadPasswordsEntryPoint::kMaxValue));

  BatchUploadService::EntryPoint entry_point = ToBatchUploadEntryPoint(
      static_cast<BatchUploadPasswordsEntryPoint>(entry_point_int));
  BatchUploadService* batch_upload =
      BatchUploadServiceFactory::GetForProfile(profile_);
  CHECK(batch_upload);
  batch_upload->OpenBatchUpload(chrome::FindBrowserWithProfile(profile_),
                                entry_point);
}
#endif

void SyncHandler::HandleGetLocalPasswordCount(const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];

  syncer::SyncService* sync_service = GetSyncService();
  if (!sync_service) {
    ResolveJavascriptCallback(callback_id, base::Value(0));
    return;
  }

  sync_service->GetLocalDataDescriptions(
      {syncer::PASSWORDS},
      base::BindOnce(&SyncHandler::HandleOnGetLocalDataDescriptionReceived,
                     base::Unretained(this), callback_id.Clone()));
}

void SyncHandler::OnStateChanged(syncer::SyncService* sync_service) {
  FireWebUIListener("trusted-vault-banner-state-changed",
                    GetTrustedVaultBannerState());
  FireWebUIListener("sync-info-changed", GetSyncInfo());

  // Only update when the `sync_service` is not configuring to avoid flickering
  // updates since the state can change between configuring and active multiple
  // times in a session.
  if (sync_service->GetTransportState() !=
      syncer::SyncService::TransportState::CONFIGURING) {
    sync_service->GetLocalDataDescriptions(
        {syncer::PASSWORDS},
        base::BindOnce(&SyncHandler::FireOnGetLocalDataDescriptionReceived,
                       base::Unretained(this)));
  }
}

void SyncHandler::FireOnGetLocalDataDescriptionReceived(
    std::map<syncer::DataType, syncer::LocalDataDescription> data) {
  int local_password_count =
      data.contains(syncer::PASSWORDS)
          ? data[syncer::PASSWORDS].local_data_models.size()
          : 0;
  FireWebUIListener("sync-service-local-password-count",
                    base::Value(local_password_count));
}

void SyncHandler::HandleOnGetLocalDataDescriptionReceived(
    base::Value callback_id,
    std::map<syncer::DataType, syncer::LocalDataDescription> data) {
  int local_password_count =
      data.contains(syncer::PASSWORDS)
          ? data[syncer::PASSWORDS].local_data_models.size()
          : 0;
  ResolveJavascriptCallback(callback_id, base::Value(local_password_count));
}

void SyncHandler::OnExtendedAccountInfoUpdated(const AccountInfo& info) {
  FireWebUIListener("stored-accounts-changed", GetAccountInfo());
}

void SyncHandler::OnExtendedAccountInfoRemoved(const AccountInfo& info) {
  FireWebUIListener("stored-accounts-changed", GetAccountInfo());
}

syncer::SyncService* SyncHandler::GetSyncService() const {
  return SyncServiceFactory::IsSyncAllowed(profile_)
             ? SyncServiceFactory::GetForProfile(profile_)
             : nullptr;
}

}  // namespace password_manager
