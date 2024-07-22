// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_sync/common_controller_builder.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/autofill/core/browser/payments/autofill_wallet_model_type_controller.h"
#include "components/autofill/core/browser/webdata/addresses/autofill_profile_sync_bridge.h"
#include "components/autofill/core/browser/webdata/addresses/contact_info_model_type_controller.h"
#include "components/autofill/core/browser/webdata/addresses/contact_info_sync_bridge.h"
#include "components/autofill/core/browser/webdata/autocomplete/autocomplete_sync_bridge.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/browser/webdata/payments/autofill_wallet_credential_sync_bridge.h"
#include "components/autofill/core/browser/webdata/payments/autofill_wallet_metadata_sync_bridge.h"
#include "components/autofill/core/browser/webdata/payments/autofill_wallet_offer_sync_bridge.h"
#include "components/autofill/core/browser/webdata/payments/autofill_wallet_sync_bridge.h"
#include "components/autofill/core/browser/webdata/payments/autofill_wallet_usage_data_sync_bridge.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/product_specifications/product_specifications_service.h"
#include "components/consent_auditor/consent_auditor.h"
#include "components/data_sharing/public/data_sharing_service.h"
#include "components/data_sharing/public/features.h"
#include "components/history/core/browser/sync/history_delete_directives_model_type_controller.h"
#include "components/history/core/browser/sync/history_model_type_controller.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/browser/sharing/incoming_password_sharing_invitation_model_type_controller.h"
#include "components/password_manager/core/browser/sharing/outgoing_password_sharing_invitation_model_type_controller.h"
#include "components/password_manager/core/browser/sharing/password_receiver_service.h"
#include "components/password_manager/core/browser/sharing/password_sender_service.h"
#include "components/password_manager/core/browser/sync/password_local_data_batch_uploader.h"
#include "components/password_manager/core/browser/sync/password_model_type_controller.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/settings/plus_address_setting_service.h"
#include "components/plus_addresses/webdata/plus_address_webdata_service.h"
#include "components/power_bookmarks/core/power_bookmark_features.h"
#include "components/power_bookmarks/core/power_bookmark_service.h"
#include "components/prefs/pref_service.h"
#include "components/reading_list/core/dual_reading_list_model.h"
#include "components/reading_list/core/reading_list_local_data_batch_uploader.h"
#include "components/send_tab_to_self/send_tab_to_self_model_type_controller.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/sync/base/features.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/forwarding_model_type_controller_delegate.h"
#include "components/sync/model/proxy_model_type_controller_delegate.h"
#include "components/sync/service/model_type_controller.h"
#include "components/sync_bookmarks/bookmark_local_data_batch_uploader.h"
#include "components/sync_bookmarks/bookmark_model_type_controller.h"
#include "components/sync_bookmarks/bookmark_sync_service.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/sync_sessions/session_model_type_controller.h"
#include "components/sync_sessions/session_sync_service.h"
#include "components/sync_user_events/user_event_model_type_controller.h"
#include "components/sync_user_events/user_event_service.h"
#include "components/variations/service/google_groups_manager.h"

#if !BUILDFLAG(IS_ANDROID)
#include "components/webauthn/core/browser/passkey_model.h"
#include "components/webauthn/core/browser/passkey_model_type_controller.h"
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "components/supervised_user/core/browser/supervised_user_settings_model_type_controller.h"
#include "components/supervised_user/core/browser/supervised_user_settings_service.h"
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USER)

namespace browser_sync {

namespace {

using syncer::ModelTypeController;
using syncer::SyncableServiceBasedModelTypeController;

// These helper functions only wrap the factory functions of the bridges. This
// way, it simplifies life for the compiler which cannot directly cast
// "WeakPtr<ModelTypeSyncBridge> (AutofillWebDataService*)" to
// "WeakPtr<ModelTypeControllerDelegate> (AutofillWebDataService*)".
base::WeakPtr<syncer::ModelTypeControllerDelegate>
AutocompleteDelegateFromDataService(autofill::AutofillWebDataService* service) {
  return autofill::AutocompleteSyncBridge::FromWebDataService(service)
      ->change_processor()
      ->GetControllerDelegate();
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
AutofillProfileDelegateFromDataService(
    autofill::AutofillWebDataService* service) {
  return autofill::AutofillProfileSyncBridge::FromWebDataService(service)
      ->change_processor()
      ->GetControllerDelegate();
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
AutofillWalletCredentialDataDelegateFromDataService(
    autofill::AutofillWebDataService* service) {
  return autofill::AutofillWalletCredentialSyncBridge::FromWebDataService(
             service)
      ->change_processor()
      ->GetControllerDelegate();
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
AutofillWalletDelegateFromDataService(
    autofill::AutofillWebDataService* service) {
  return autofill::AutofillWalletSyncBridge::FromWebDataService(service)
      ->change_processor()
      ->GetControllerDelegate();
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
AutofillWalletMetadataDelegateFromDataService(
    autofill::AutofillWebDataService* service) {
  return autofill::AutofillWalletMetadataSyncBridge::FromWebDataService(service)
      ->change_processor()
      ->GetControllerDelegate();
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
AutofillWalletOfferDelegateFromDataService(
    autofill::AutofillWebDataService* service) {
  return autofill::AutofillWalletOfferSyncBridge::FromWebDataService(service)
      ->change_processor()
      ->GetControllerDelegate();
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
AutofillWalletUsageDataDelegateFromDataService(
    autofill::AutofillWebDataService* service) {
  return autofill::AutofillWalletUsageDataSyncBridge::FromWebDataService(
             service)
      ->change_processor()
      ->GetControllerDelegate();
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
ContactInfoDelegateFromDataService(autofill::AutofillWebDataService* service) {
  return autofill::ContactInfoSyncBridge::FromWebDataService(service)
      ->change_processor()
      ->GetControllerDelegate();
}

// Helper function that deals will null (e.g. tests, iOS webview).
base::WeakPtr<syncer::SyncableService> SyncableServiceForPrefs(
    sync_preferences::PrefServiceSyncable* prefs_service,
    syncer::ModelType type) {
  return prefs_service ? prefs_service->GetSyncableService(type)->AsWeakPtr()
                       : nullptr;
}

}  // namespace

CommonControllerBuilder::CommonControllerBuilder() = default;

CommonControllerBuilder::~CommonControllerBuilder() = default;

void CommonControllerBuilder::SetAutofillWebDataService(
    const scoped_refptr<base::SequencedTaskRunner>& ui_thread,
    const scoped_refptr<base::SequencedTaskRunner>& db_thread,
    const scoped_refptr<autofill::AutofillWebDataService>&
        web_data_service_on_disk,
    const scoped_refptr<autofill::AutofillWebDataService>&
        web_data_service_in_memory) {
  autofill_web_data_ui_thread_.Set(ui_thread);
  autofill_web_data_db_thread_.Set(db_thread);
  autofill_web_data_service_on_disk_.Set(web_data_service_on_disk);
  autofill_web_data_service_in_memory_.Set(web_data_service_in_memory);
}

void CommonControllerBuilder::SetBookmarkModel(
    bookmarks::BookmarkModel* bookmark_model) {
  bookmark_model_.Set(bookmark_model);
}

void CommonControllerBuilder::SetBookmarkSyncService(
    sync_bookmarks::BookmarkSyncService*
        local_or_syncable_bookmark_sync_service,
    sync_bookmarks::BookmarkSyncService* account_bookmark_sync_service) {
  local_or_syncable_bookmark_sync_service_.Set(
      local_or_syncable_bookmark_sync_service);
  account_bookmark_sync_service_.Set(account_bookmark_sync_service);
}

void CommonControllerBuilder::SetConsentAuditor(
    consent_auditor::ConsentAuditor* consent_auditor) {
  consent_auditor_.Set(consent_auditor);
}

void CommonControllerBuilder::SetDataSharingService(
    data_sharing::DataSharingService* data_sharing_service) {
  data_sharing_service_.Set(data_sharing_service);
}

void CommonControllerBuilder::SetDeviceInfoSyncService(
    syncer::DeviceInfoSyncService* device_info_sync_service) {
  device_info_sync_service_.Set(device_info_sync_service);
}

void CommonControllerBuilder::SetFaviconService(
    favicon::FaviconService* favicon_service) {
  favicon_service_.Set(favicon_service);
}

void CommonControllerBuilder::SetGoogleGroupsManager(
    GoogleGroupsManager* google_groups_manager) {
  google_groups_manager_.Set(google_groups_manager);
}

void CommonControllerBuilder::SetHistoryService(
    history::HistoryService* history_service) {
  history_service_.Set(history_service);
}

void CommonControllerBuilder::SetIdentityManager(
    signin::IdentityManager* identity_manager) {
  identity_manager_.Set(identity_manager);
}

void CommonControllerBuilder::SetModelTypeStoreService(
    syncer::ModelTypeStoreService* model_type_store_service) {
  model_type_store_service_.Set(model_type_store_service);
}

#if !BUILDFLAG(IS_ANDROID)
void CommonControllerBuilder::SetPasskeyModel(
    webauthn::PasskeyModel* passkey_model) {
  passkey_model_.Set(passkey_model);
}
#endif  // !BUILDFLAG(IS_ANDROID)

void CommonControllerBuilder::SetPasswordReceiverService(
    password_manager::PasswordReceiverService* password_receiver_service) {
  password_receiver_service_.Set(password_receiver_service);
}

void CommonControllerBuilder::SetPasswordSenderService(
    password_manager::PasswordSenderService* password_sender_service) {
  password_sender_service_.Set(password_sender_service);
}

void CommonControllerBuilder::SetPasswordStore(
    const scoped_refptr<password_manager::PasswordStoreInterface>&
        profile_password_store,
    const scoped_refptr<password_manager::PasswordStoreInterface>&
        account_password_store) {
  profile_password_store_.Set(profile_password_store);
  account_password_store_.Set(account_password_store);
}

void CommonControllerBuilder::SetPlusAddressServices(
    plus_addresses::PlusAddressSettingService* plus_address_setting_service,
    const scoped_refptr<plus_addresses::PlusAddressWebDataService>&
        plus_address_webdata_service) {
  plus_address_setting_service_.Set(plus_address_setting_service);
  plus_address_webdata_service_.Set(plus_address_webdata_service);
}

void CommonControllerBuilder::SetPowerBookmarkService(
    power_bookmarks::PowerBookmarkService* power_bookmark_service) {
  power_bookmark_service_.Set(power_bookmark_service);
}

void CommonControllerBuilder::SetPrefService(PrefService* pref_service) {
  pref_service_.Set(pref_service);
}

void CommonControllerBuilder::SetPrefServiceSyncable(
    sync_preferences::PrefServiceSyncable* pref_service_syncable) {
  pref_service_syncable_.Set(pref_service_syncable);
}

void CommonControllerBuilder::SetProductSpecificationsService(
    commerce::ProductSpecificationsService* product_specifications_service) {
  product_specifications_service_.Set(product_specifications_service);
}

void CommonControllerBuilder::SetDualReadingListModel(
    reading_list::DualReadingListModel* dual_reading_list_model) {
  dual_reading_list_model_.Set(dual_reading_list_model);
}

void CommonControllerBuilder::SetSendTabToSelfSyncService(
    send_tab_to_self::SendTabToSelfSyncService* send_tab_to_self_sync_service) {
  send_tab_to_self_sync_service_.Set(send_tab_to_self_sync_service);
}

void CommonControllerBuilder::SetSessionSyncService(
    sync_sessions::SessionSyncService* session_sync_service) {
  session_sync_service_.Set(session_sync_service);
}

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
void CommonControllerBuilder::SetSupervisedUserSettingsService(
    supervised_user::SupervisedUserSettingsService*
        supervised_user_settings_service) {
  supervised_user_settings_service_.Set(supervised_user_settings_service);
}
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)

void CommonControllerBuilder::SetUserEventService(
    syncer::UserEventService* user_event_service) {
  user_event_service_.Set(user_event_service);
}

std::vector<std::unique_ptr<syncer::ModelTypeController>>
CommonControllerBuilder::Build(syncer::ModelTypeSet disabled_types,
                               syncer::SyncService* sync_service,
                               version_info::Channel channel) {
  std::vector<std::unique_ptr<syncer::ModelTypeController>> controllers;

  const base::RepeatingClosure dump_stack =
      base::BindRepeating(&syncer::ReportUnrecoverableError, channel);

  // Same delegate for full-sync or transport mode.
  controllers.push_back(std::make_unique<ModelTypeController>(
      syncer::DEVICE_INFO,
      /*delegate_for_full_sync_mode=*/
      std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
          device_info_sync_service_.value()->GetControllerDelegate().get()),
      /*delegate_for_transport_mode=*/
      std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
          device_info_sync_service_.value()->GetControllerDelegate().get())));

  // These features are enabled only if there's a DB thread to post tasks to.
  if (autofill_web_data_db_thread_.value()) {
    if (!disabled_types.Has(syncer::AUTOFILL)) {
      // Note: Transport mode is not and will not be supported.
      controllers.push_back(std::make_unique<ModelTypeController>(
          syncer::AUTOFILL,
          std::make_unique<syncer::ProxyModelTypeControllerDelegate>(
              autofill_web_data_db_thread_.value(),
              base::BindRepeating(
                  &AutocompleteDelegateFromDataService,
                  base::RetainedRef(
                      autofill_web_data_service_on_disk_.value()))),
          /*delegate_for_transport_mode=*/nullptr));
    }

    if (!disabled_types.Has(syncer::AUTOFILL_PROFILE)) {
      // Note: Transport mode is not and will not be supported - support is
      // coming via CONTACT_INFO instead.
      controllers.push_back(std::make_unique<syncer::ModelTypeController>(
          syncer::AUTOFILL_PROFILE,
          std::make_unique<syncer::ProxyModelTypeControllerDelegate>(
              autofill_web_data_db_thread_.value(),
              base::BindRepeating(
                  &AutofillProfileDelegateFromDataService,
                  base::RetainedRef(
                      autofill_web_data_service_on_disk_.value()))),
          /*delegate_for_transport_mode=*/nullptr));
    }

    if (!disabled_types.Has(syncer::CONTACT_INFO)) {
      // The same delegate is used for full sync and transport mode.
      controllers.push_back(
          std::make_unique<autofill::ContactInfoModelTypeController>(
              /*delegate_for_full_sync_mode=*/
              std::make_unique<syncer::ProxyModelTypeControllerDelegate>(
                  autofill_web_data_db_thread_.value(),
                  base::BindRepeating(
                      &ContactInfoDelegateFromDataService,
                      base::RetainedRef(
                          autofill_web_data_service_on_disk_.value()))),
              /*delegate_for_transport_mode=*/
              std::make_unique<syncer::ProxyModelTypeControllerDelegate>(
                  autofill_web_data_db_thread_.value(),
                  base::BindRepeating(
                      &ContactInfoDelegateFromDataService,
                      base::RetainedRef(
                          autofill_web_data_service_on_disk_.value()))),
              sync_service, identity_manager_.value()));
    }

    if (!disabled_types.Has(syncer::AUTOFILL_WALLET_DATA)) {
      controllers.push_back(CreateWalletModelTypeController(
          syncer::AUTOFILL_WALLET_DATA,
          base::BindRepeating(&AutofillWalletDelegateFromDataService),
          sync_service, /*with_transport_mode_support=*/true));
    }

    // Wallet metadata sync depends on Wallet data sync.
    if (!disabled_types.Has(syncer::AUTOFILL_WALLET_DATA) &&
        !disabled_types.Has(syncer::AUTOFILL_WALLET_METADATA)) {
      controllers.push_back(CreateWalletModelTypeController(
          syncer::AUTOFILL_WALLET_METADATA,
          base::BindRepeating(&AutofillWalletMetadataDelegateFromDataService),
          sync_service, /*with_transport_mode_support=*/
          base::FeatureList::IsEnabled(
              syncer::kSyncEnableWalletMetadataInTransportMode)));
    }

    // Wallet offer sync depends on Wallet data sync.
    if (!disabled_types.Has(syncer::AUTOFILL_WALLET_DATA) &&
        !disabled_types.Has(syncer::AUTOFILL_WALLET_OFFER)) {
      controllers.push_back(CreateWalletModelTypeController(
          syncer::AUTOFILL_WALLET_OFFER,
          base::BindRepeating(&AutofillWalletOfferDelegateFromDataService),
          sync_service, /*with_transport_mode_support=*/
          base::FeatureList::IsEnabled(
              syncer::kSyncEnableWalletOfferInTransportMode)));
    }

    // Wallet usage data sync depends on Wallet data sync.
    if (base::FeatureList::IsEnabled(syncer::kSyncAutofillWalletUsageData) &&
        !disabled_types.Has(syncer::AUTOFILL_WALLET_DATA) &&
        !disabled_types.Has(syncer::AUTOFILL_WALLET_USAGE)) {
      controllers.push_back(CreateWalletModelTypeController(
          syncer::AUTOFILL_WALLET_USAGE,
          base::BindRepeating(&AutofillWalletUsageDataDelegateFromDataService),
          sync_service, /*with_transport_mode_support=*/true));
    }

    // Wallet credential data sync depends on Wallet data sync.
    if (base::FeatureList::IsEnabled(
            syncer::kSyncAutofillWalletCredentialData) &&
        !disabled_types.Has(syncer::AUTOFILL_WALLET_DATA) &&
        !disabled_types.Has(syncer::AUTOFILL_WALLET_CREDENTIAL)) {
      controllers.push_back(CreateWalletModelTypeController(
          syncer::AUTOFILL_WALLET_CREDENTIAL,
          base::BindRepeating(
              &AutofillWalletCredentialDataDelegateFromDataService),
          sync_service, /*with_transport_mode_support=*/true));
    }
  }

  if (!disabled_types.Has(syncer::BOOKMARKS)) {
    // Services can be null in tests.
    if (local_or_syncable_bookmark_sync_service_.value() &&
        favicon_service_.value()) {
      auto full_mode_delegate =
          std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
              local_or_syncable_bookmark_sync_service_.value()
                  ->GetBookmarkSyncControllerDelegate(favicon_service_.value())
                  .get());
      auto transport_mode_delegate =
          account_bookmark_sync_service_.value()
              ? std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
                    account_bookmark_sync_service_.value()
                        ->GetBookmarkSyncControllerDelegate(
                            favicon_service_.value())
                        .get())
              : nullptr;
      controllers.push_back(
          std::make_unique<sync_bookmarks::BookmarkModelTypeController>(
              std::move(full_mode_delegate), std::move(transport_mode_delegate),
              std::make_unique<sync_bookmarks::BookmarkLocalDataBatchUploader>(
                  bookmark_model_.value())));
    }

    if (!disabled_types.Has(syncer::POWER_BOOKMARK) &&
        power_bookmark_service_.value() &&
        base::FeatureList::IsEnabled(power_bookmarks::kPowerBookmarkBackend)) {
      // TODO(crbug.com/40261319): Support transport mode for POWER_BOOKMARK.
      controllers.push_back(std::make_unique<ModelTypeController>(
          syncer::POWER_BOOKMARK,
          power_bookmark_service_.value()->CreateSyncControllerDelegate(),
          /*delegate_for_transport_mode=*/nullptr));
    }
  }

  if (!disabled_types.Has(syncer::PRODUCT_COMPARISON) &&
      product_specifications_service_.value() &&
      base::FeatureList::IsEnabled(commerce::kProductSpecifications)) {
    syncer::ModelTypeControllerDelegate* delegate =
        product_specifications_service_.value()
            ->GetSyncControllerDelegate()
            .get();
    controllers.push_back(std::make_unique<ModelTypeController>(
        syncer::PRODUCT_COMPARISON,
        std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
            delegate),
        /*delegate_for_transport_mode= */
        std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
            delegate)));
  }

  if (!disabled_types.Has(syncer::HISTORY)) {
    controllers.push_back(std::make_unique<history::HistoryModelTypeController>(
        sync_service, identity_manager_.value(), history_service_.value(),
        pref_service_.value()));
  }

  if (!disabled_types.Has(syncer::HISTORY_DELETE_DIRECTIVES)) {
    controllers.push_back(
        std::make_unique<history::HistoryDeleteDirectivesModelTypeController>(
            dump_stack, sync_service, model_type_store_service_.value(),
            history_service_.value(), pref_service_.value()));
  }

  if (!disabled_types.Has(syncer::SESSIONS)) {
    syncer::ModelTypeControllerDelegate* delegate =
        session_sync_service_.value()->GetControllerDelegate().get();
    auto full_sync_mode_delegate =
        std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
            delegate);
    auto transport_mode_delegate =
        base::FeatureList::IsEnabled(syncer::kReplaceSyncPromosWithSignInPromos)
            ? std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
                  delegate)
            : nullptr;
    controllers.push_back(
        std::make_unique<sync_sessions::SessionModelTypeController>(
            sync_service, pref_service_.value(),
            std::move(full_sync_mode_delegate),
            std::move(transport_mode_delegate)));
  }

  if (!disabled_types.Has(syncer::PASSWORDS)) {
    if (profile_password_store_.value()) {
      // |profile_password_store_| can be null in tests.
      controllers.push_back(std::make_unique<
                            password_manager::PasswordModelTypeController>(
          profile_password_store_.value()->CreateSyncControllerDelegate(),
          account_password_store_.value()
              ? account_password_store_.value()->CreateSyncControllerDelegate()
              : nullptr,
          std::make_unique<password_manager::PasswordLocalDataBatchUploader>(
              profile_password_store_.value(), account_password_store_.value()),
          pref_service_.value(), identity_manager_.value(), sync_service));

      // Couple password sharing invitations with password data type.
      if (!disabled_types.Has(syncer::INCOMING_PASSWORD_SHARING_INVITATION) &&
          password_receiver_service_.value()) {
        controllers.push_back(
            std::make_unique<
                password_manager::
                    IncomingPasswordSharingInvitationModelTypeController>(
                sync_service, password_receiver_service_.value(),
                pref_service_.value()));
      }

      if (!disabled_types.Has(syncer::OUTGOING_PASSWORD_SHARING_INVITATION) &&
          password_sender_service_.value()) {
        controllers.push_back(
            std::make_unique<
                password_manager::
                    OutgoingPasswordSharingInvitationModelTypeController>(
                sync_service, password_sender_service_.value(),
                pref_service_.value()));
      }
    }
  }

  // `plus_address_webdata_service_` is null on iOS WebView.
  // `kEnterprisePlusAddressServerUrl` is checked to prevent enabling the
  // feature in dev builds via the field trial config.
  if (!disabled_types.Has(syncer::PLUS_ADDRESS) &&
      plus_address_webdata_service_.value() && google_groups_manager_.value() &&
      google_groups_manager_.value()->IsFeatureEnabledForProfile(
          plus_addresses::features::kPlusAddressesEnabled) &&
      !plus_addresses::features::kEnterprisePlusAddressServerUrl.Get()
           .empty() &&
      base::FeatureList::IsEnabled(syncer::kSyncPlusAddress)) {
    controllers.push_back(std::make_unique<syncer::ModelTypeController>(
        syncer::PLUS_ADDRESS,
        /*delegate_for_full_sync_mode=*/
        plus_address_webdata_service_.value()->GetSyncControllerDelegate(),
        /*delegate_for_transport_mode=*/
        plus_address_webdata_service_.value()->GetSyncControllerDelegate()));
  }

  // `plus_address_setting_service_` is null on iOS WebView.
  // `kEnterprisePlusAddressServerUrl` is checked to prevent enabling the
  // feature in dev builds via the field trial config.
  if (!disabled_types.Has(syncer::PLUS_ADDRESS_SETTING) &&
      plus_address_setting_service_.value() && google_groups_manager_.value() &&
      google_groups_manager_.value()->IsFeatureEnabledForProfile(
          plus_addresses::features::kPlusAddressesEnabled) &&
      !plus_addresses::features::kEnterprisePlusAddressServerUrl.Get()
           .empty() &&
      base::FeatureList::IsEnabled(syncer::kSyncPlusAddressSetting)) {
    controllers.push_back(std::make_unique<syncer::ModelTypeController>(
        syncer::PLUS_ADDRESS_SETTING,
        /*delegate_for_full_sync_mode=*/
        plus_address_setting_service_.value()->GetSyncControllerDelegate(),
        /*delegate_for_transport_mode=*/
        plus_address_setting_service_.value()->GetSyncControllerDelegate()));
  }

  if (!disabled_types.Has(syncer::PREFERENCES)) {
    bool allow_transport_mode =
        base::FeatureList::IsEnabled(
            syncer::kReplaceSyncPromosWithSignInPromos) &&
        base::FeatureList::IsEnabled(syncer::kEnablePreferencesAccountStorage);
    controllers.push_back(
        std::make_unique<SyncableServiceBasedModelTypeController>(
            syncer::PREFERENCES,
            model_type_store_service_.value()->GetStoreFactory(),
            SyncableServiceForPrefs(pref_service_syncable_.value(),
                                    syncer::PREFERENCES),
            dump_stack,
            allow_transport_mode
                ? SyncableServiceBasedModelTypeController::DelegateMode::
                      kTransportModeWithSingleModel
                : SyncableServiceBasedModelTypeController::DelegateMode::
                      kLegacyFullSyncModeOnly));
  }

  if (!disabled_types.Has(syncer::PRIORITY_PREFERENCES)) {
    bool allow_transport_mode =
        base::FeatureList::IsEnabled(
            syncer::kReplaceSyncPromosWithSignInPromos) &&
        base::FeatureList::IsEnabled(syncer::kEnablePreferencesAccountStorage);
    controllers.push_back(
        std::make_unique<SyncableServiceBasedModelTypeController>(
            syncer::PRIORITY_PREFERENCES,
            model_type_store_service_.value()->GetStoreFactory(),
            SyncableServiceForPrefs(pref_service_syncable_.value(),
                                    syncer::PRIORITY_PREFERENCES),
            dump_stack,
            allow_transport_mode
                ? SyncableServiceBasedModelTypeController::DelegateMode::
                      kTransportModeWithSingleModel
                : SyncableServiceBasedModelTypeController::DelegateMode::
                      kLegacyFullSyncModeOnly));
  }

  if (!disabled_types.Has(syncer::READING_LIST)) {
    // The transport-mode delegate may or may not be null depending on
    // platform and feature toggle state.
    syncer::ModelTypeControllerDelegate* delegate_for_transport_mode =
        dual_reading_list_model_.value()
            ->GetSyncControllerDelegateForTransportMode()
            .get();

    controllers.push_back(std::make_unique<ModelTypeController>(
        syncer::READING_LIST,
        /*delegate_for_full_sync_mode=*/
        std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
            dual_reading_list_model_.value()
                ->GetSyncControllerDelegate()
                .get()),
        /*delegate_for_transport_mode=*/
        delegate_for_transport_mode
            ? std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
                  delegate_for_transport_mode)
            : nullptr,
        std::make_unique<reading_list::ReadingListLocalDataBatchUploader>(
            dual_reading_list_model_.value())));
  }

  if (!disabled_types.Has(syncer::USER_EVENTS)) {
    syncer::ModelTypeControllerDelegate* delegate =
        user_event_service_.value()->GetControllerDelegate().get();

    controllers.push_back(std::make_unique<
                          syncer::UserEventModelTypeController>(
        sync_service,
        /*delegate_for_full_sync_mode=*/
        std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
            delegate),
        /*delegate_for_transport_mode=*/
        base::FeatureList::IsEnabled(syncer::kReplaceSyncPromosWithSignInPromos)
            ? std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
                  delegate)
            : nullptr));
  }

  if (!disabled_types.Has(syncer::SEND_TAB_TO_SELF)) {
    syncer::ModelTypeControllerDelegate* delegate =
        send_tab_to_self_sync_service_.value()->GetControllerDelegate().get();
    controllers.push_back(
        std::make_unique<send_tab_to_self::SendTabToSelfModelTypeController>(
            /*delegate_for_full_sync_mode=*/
            std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
                delegate),
            /*delegate_for_transport_mode=*/
            std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
                delegate)));
  }

  if (!disabled_types.Has(syncer::USER_CONSENTS)) {
    syncer::ModelTypeControllerDelegate* delegate =
        consent_auditor_.value()->GetControllerDelegate().get();

    // Forward both full-sync and transport-only modes to the same delegate,
    // since behavior for USER_CONSENTS does not differ (they are always
    // persisted).
    controllers.push_back(std::make_unique<ModelTypeController>(
        syncer::USER_CONSENTS,
        /*delegate_for_full_sync_mode=*/
        std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
            delegate),
        /*delegate_for_transport_mode=*/
        std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
            delegate)));
  }

#if !BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(syncer::kSyncWebauthnCredentials) &&
      !disabled_types.Has(syncer::WEBAUTHN_CREDENTIAL)) {
    syncer::ModelTypeControllerDelegate* delegate =
        passkey_model_.value()->GetModelTypeControllerDelegate().get();

    controllers.push_back(
        std::make_unique<webauthn::PasskeyModelTypeController>(
            sync_service,
            /*delegate_for_full_sync_mode=*/
            std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
                delegate),
            /*delegate_for_transport_mode=*/
            std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
                delegate)));
  }
#endif

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  if (supervised_user_settings_service_.value()) {
    controllers.push_back(
        std::make_unique<SupervisedUserSettingsModelTypeController>(
            dump_stack, model_type_store_service_.value()->GetStoreFactory(),
            supervised_user_settings_service_.value()->AsWeakPtr(),
            pref_service_.value()));
  }
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)

  // `data_sharing_service_` is null on iOS WebView.
  if (data_sharing_service_.value() &&
      base::FeatureList::IsEnabled(
          data_sharing::features::kDataSharingFeature) &&
      !disabled_types.Has(syncer::COLLABORATION_GROUP)) {
    syncer::ModelTypeControllerDelegate* delegate =
        data_sharing_service_.value()
            ->GetCollaborationGroupControllerDelegate()
            .get();

    controllers.push_back(std::make_unique<ModelTypeController>(
        syncer::COLLABORATION_GROUP,
        /*delegate_for_full_sync_mode=*/
        std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
            delegate),
        /*delegate_for_transport_mode=*/
        std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
            delegate)));
  }

  // TODO(crbug.com/335688372): Temporary workaround to avoid test failures in
  // some browser tests that override factories late, which otherwise runs into
  // dangling raw pointers.
  passkey_model_.Reset();
  consent_auditor_.Reset();

  return controllers;
}

std::unique_ptr<ModelTypeController>
CommonControllerBuilder::CreateWalletModelTypeController(
    syncer::ModelType type,
    const base::RepeatingCallback<
        base::WeakPtr<syncer::ModelTypeControllerDelegate>(
            autofill::AutofillWebDataService*)>& delegate_from_web_data,
    syncer::SyncService* sync_service,
    bool with_transport_mode_support) {
  // Transport mode should be supported, except for METADATA and OFFER where
  // support is still work in progress, see crbug.com/1448894 and
  // crbug.com/1448895.
  CHECK(with_transport_mode_support ||
        type == syncer::AUTOFILL_WALLET_METADATA ||
        type == syncer::AUTOFILL_WALLET_OFFER);
  auto delegate_for_full_sync_mode =
      std::make_unique<syncer::ProxyModelTypeControllerDelegate>(
          autofill_web_data_db_thread_.value(),
          base::BindRepeating(
              delegate_from_web_data,
              base::RetainedRef(autofill_web_data_service_on_disk_.value())));
  auto delegate_for_transport_mode =
      with_transport_mode_support
          ? std::make_unique<syncer::ProxyModelTypeControllerDelegate>(
                autofill_web_data_db_thread_.value(),
                base::BindRepeating(
                    delegate_from_web_data,
                    base::RetainedRef(
                        autofill_web_data_service_in_memory_.value())))
          : nullptr;
  return std::make_unique<AutofillWalletModelTypeController>(
      type, std::move(delegate_for_full_sync_mode),
      std::move(delegate_for_transport_mode), pref_service_.value(),
      sync_service);
}

}  // namespace browser_sync
