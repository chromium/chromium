// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_sync/common_controller_builder.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/account_settings/account_setting_service.h"
#include "components/autofill/core/browser/payments/autofill_wallet_data_type_controller.h"
#include "components/autofill/core/browser/webdata/addresses/autofill_profile_sync_bridge.h"
#include "components/autofill/core/browser/webdata/addresses/contact_info_data_type_controller.h"
#include "components/autofill/core/browser/webdata/addresses/contact_info_local_data_batch_uploader.h"
#include "components/autofill/core/browser/webdata/addresses/contact_info_sync_bridge.h"
#include "components/autofill/core/browser/webdata/autocomplete/autocomplete_sync_bridge.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/browser/webdata/payments/autofill_wallet_credential_sync_bridge.h"
#include "components/autofill/core/browser/webdata/payments/autofill_wallet_metadata_sync_bridge.h"
#include "components/autofill/core/browser/webdata/payments/autofill_wallet_offer_sync_bridge.h"
#include "components/autofill/core/browser/webdata/payments/autofill_wallet_sync_bridge.h"
#include "components/autofill/core/browser/webdata/payments/autofill_wallet_usage_data_sync_bridge.h"
#include "components/autofill/core/browser/webdata/valuables/valuable_data_type_controller.h"
#include "components/autofill/core/browser/webdata/valuables/valuable_metadata_sync_bridge.h"
#include "components/autofill/core/browser/webdata/valuables/valuable_sync_bridge.h"
#include "components/collaboration/public/collaboration_service.h"
#include "components/collaboration/public/data_type_controller/collaboration_group_data_type_controller.h"
#include "components/collaboration/public/data_type_controller/shared_tab_group_account_data_type_controller.h"
#include "components/collaboration/public/data_type_controller/shared_tab_group_data_type_controller.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/consent_auditor/consent_auditor.h"
#include "components/contextual_tasks/public/ai_thread_data_type_controller.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/gemini_thread_data_type_controller.h"
#include "components/data_sharing/public/data_sharing_service.h"
#include "components/data_sharing/public/features.h"
#include "components/data_sharing/public/personal_collaboration_data/personal_collaboration_data_service.h"
#include "components/history/core/browser/sync/history_data_type_controller.h"
#include "components/history/core/browser/sync/history_delete_directives_data_type_controller.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/browser/sharing/incoming_password_sharing_invitation_data_type_controller.h"
#include "components/password_manager/core/browser/sharing/outgoing_password_sharing_invitation_data_type_controller.h"
#include "components/password_manager/core/browser/sharing/password_receiver_service.h"
#include "components/password_manager/core/browser/sharing/password_sender_service.h"
#include "components/password_manager/core/browser/sync/password_data_type_controller.h"
#include "components/password_manager/core/browser/sync/password_local_data_batch_uploader.h"
#include "components/plus_addresses/core/browser/settings/plus_address_setting_service.h"
#include "components/plus_addresses/core/browser/sync_utils/plus_address_data_type_controller.h"
#include "components/plus_addresses/core/browser/webdata/plus_address_webdata_service.h"
#include "components/prefs/pref_service.h"
#include "components/reading_list/core/dual_reading_list_model.h"
#include "components/reading_list/core/reading_list_local_data_batch_uploader.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/search_engines/template_url_service.h"
#include "components/send_tab_to_self/send_tab_to_self_data_type_controller.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/sharing_message/sharing_message_bridge.h"
#include "components/sharing_message/sharing_message_data_type_controller.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/skills/features.h"
#include "components/skills/public/skills_service.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/forwarding_data_type_controller_delegate.h"
#include "components/sync/model/proxy_data_type_controller_delegate.h"
#include "components/sync/service/data_type_controller.h"
#include "components/sync_bookmarks/bookmark_data_type_controller.h"
#include "components/sync_bookmarks/bookmark_local_data_batch_uploader.h"
#include "components/sync_bookmarks/bookmark_sync_service.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/sync_sessions/session_data_type_controller.h"
#include "components/sync_sessions/session_sync_service.h"
#include "components/sync_user_events/user_event_data_type_controller.h"
#include "components/sync_user_events/user_event_service.h"
#include "components/variations/service/google_groups_manager.h"

#if !BUILDFLAG(IS_ANDROID)
#include "components/webauthn/core/browser/passkey_data_type_controller.h"
#include "components/webauthn/core/browser/passkey_model.h"
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "components/supervised_user/core/browser/family_link_settings_data_type_controller.h"
#include "components/supervised_user/core/browser/family_link_settings_service.h"
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USER)

namespace browser_sync {

namespace {

using syncer::DataTypeController;
using syncer::SyncableServiceBasedDataTypeController;

// These helper functions only wrap the factory functions of the bridges. This
// way, it simplifies life for the compiler which cannot directly cast
// "WeakPtr<DataTypeSyncBridge> (AutofillWebDataService*)" to
// "WeakPtr<DataTypeControllerDelegate> (AutofillWebDataService*)".
base::WeakPtr<syncer::DataTypeControllerDelegate>
AutocompleteDelegateFromDataService(autofill::AutofillWebDataService* service) {
  return autofill::AutocompleteSyncBridge::FromWebDataService(service)
      ->change_processor()
      ->GetControllerDelegate();
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
AutofillValuableDelegateFromDataService(
    autofill::AutofillWebDataService* service) {
  return autofill::ValuableSyncBridge::FromWebDataService(service)
      ->change_processor()
      ->GetControllerDelegate();
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
AutofillValuableMetadataDelegateFromDataService(
    autofill::AutofillWebDataService* service) {
  return autofill::ValuableMetadataSyncBridge::FromWebDataService(service)
      ->change_processor()
      ->GetControllerDelegate();
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
AutofillProfileDelegateFromDataService(
    autofill::AutofillWebDataService* service) {
  return autofill::AutofillProfileSyncBridge::FromWebDataService(service)
      ->change_processor()
      ->GetControllerDelegate();
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
AutofillWalletCredentialDataDelegateFromDataService(
    autofill::AutofillWebDataService* service) {
  return autofill::AutofillWalletCredentialSyncBridge::FromWebDataService(
             service)
      ->change_processor()
      ->GetControllerDelegate();
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
AutofillWalletDelegateFromDataService(
    autofill::AutofillWebDataService* service) {
  return autofill::AutofillWalletSyncBridge::FromWebDataService(service)
      ->change_processor()
      ->GetControllerDelegate();
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
AutofillWalletMetadataDelegateFromDataService(
    autofill::AutofillWebDataService* service) {
  return autofill::AutofillWalletMetadataSyncBridge::FromWebDataService(service)
      ->change_processor()
      ->GetControllerDelegate();
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
AutofillWalletOfferDelegateFromDataService(
    autofill::AutofillWebDataService* service) {
  return autofill::AutofillWalletOfferSyncBridge::FromWebDataService(service)
      ->change_processor()
      ->GetControllerDelegate();
}

#if !BUILDFLAG(IS_IOS)
base::WeakPtr<syncer::DataTypeControllerDelegate>
AutofillWalletUsageDataDelegateFromDataService(
    autofill::AutofillWebDataService* service) {
  return autofill::AutofillWalletUsageDataSyncBridge::FromWebDataService(
             service)
      ->change_processor()
      ->GetControllerDelegate();
}
#endif

base::WeakPtr<syncer::DataTypeControllerDelegate>
ContactInfoDelegateFromDataService(autofill::AutofillWebDataService* service) {
  return autofill::ContactInfoSyncBridge::FromWebDataService(service)
      ->change_processor()
      ->GetControllerDelegate();
}

// Helper function that deals will null (e.g. tests, iOS webview).
base::WeakPtr<syncer::SyncableService> SyncableServiceForPrefs(
    sync_preferences::PrefServiceSyncable* prefs_service,
    syncer::DataType type) {
  return prefs_service ? prefs_service->GetSyncableService(type)->AsWeakPtr()
                       : nullptr;
}

bool ArePreferencesAllowedInTransportMode() {
  if (!base::FeatureList::IsEnabled(
          switches::kEnablePreferencesAccountStorage)) {
    return false;
  }
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  return syncer::IsReplaceSyncPromosWithSignInPromosEnabled();
#else
  return true;
#endif
}

}  // namespace

CommonControllerBuilder::CommonControllerBuilder() = default;

CommonControllerBuilder::~CommonControllerBuilder() = default;

void CommonControllerBuilder::SetAccountSettingService(
    account_settings::AccountSettingService* account_setting_service) {
  account_setting_service_.Set(account_setting_service);
}

void CommonControllerBuilder::SetAddressDataManagerGetter(
    base::RepeatingCallback<autofill::AddressDataManager*()>
        address_data_manager_getter) {
  address_data_manager_getter_ = std::move(address_data_manager_getter);
}

void CommonControllerBuilder::SetAutofillWebDataService(
    const scoped_refptr<base::SequencedTaskRunner>& ui_thread,
    const scoped_refptr<autofill::AutofillWebDataService>&
        web_data_service_on_disk,
    const scoped_refptr<autofill::AutofillWebDataService>&
        web_data_service_in_memory) {
  autofill_web_data_ui_thread_.Set(ui_thread);
  profile_autofill_web_data_service_.Set(web_data_service_on_disk);
  account_autofill_web_data_service_.Set(web_data_service_in_memory);
}

void CommonControllerBuilder::SetAimEligibilityService(
    AimEligibilityService* aim_eligibility_service) {
  aim_eligibility_service_.Set(aim_eligibility_service);
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

void CommonControllerBuilder::SetCollaborationService(
    collaboration::CollaborationService* collaboration_service) {
  collaboration_service_.Set(collaboration_service);
}

void CommonControllerBuilder::SetContextualTasksService(
    contextual_tasks::ContextualTasksService* contextual_tasks_service) {
  contextual_tasks_service_.Set(contextual_tasks_service);
}

void CommonControllerBuilder::SetPersonalCollaborationDataService(
    data_sharing::personal_collaboration_data::PersonalCollaborationDataService*
        personal_collaboration_data_service) {
  personal_collaboration_data_service_.Set(personal_collaboration_data_service);
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

void CommonControllerBuilder::SetDataTypeStoreService(
    syncer::DataTypeStoreService* data_type_store_service) {
  data_type_store_service_.Set(data_type_store_service);
}

void CommonControllerBuilder::SetSkillsService(
    skills::SkillsService* skills_service) {
  skills_service_.Set(skills_service);
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

void CommonControllerBuilder::SetPrefService(PrefService* pref_service) {
  pref_service_.Set(pref_service);
}

void CommonControllerBuilder::SetPrefServiceSyncable(
    sync_preferences::PrefServiceSyncable* pref_service_syncable) {
  pref_service_syncable_.Set(pref_service_syncable);
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

void CommonControllerBuilder::SetSharingMessageBridge(
    SharingMessageBridge* sharing_message_bridge) {
  sharing_message_bridge_.Set(sharing_message_bridge);
}

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
void CommonControllerBuilder::SetFamilyLinkSettingsService(
    supervised_user::FamilyLinkSettingsService* family_link_settings_service) {
  family_link_settings_service_.Set(family_link_settings_service);
}
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)

void CommonControllerBuilder::SetTabGroupSyncService(
    tab_groups::TabGroupSyncService* tab_group_sync_service) {
  tab_group_sync_service_.Set(tab_group_sync_service);
}

void CommonControllerBuilder::SetTemplateURLService(
    TemplateURLService* template_url_service) {
  template_url_service_.Set(template_url_service);
}

void CommonControllerBuilder::SetUserEventService(
    syncer::UserEventService* user_event_service) {
  user_event_service_.Set(user_event_service);
}

std::vector<std::unique_ptr<syncer::DataTypeController>>
CommonControllerBuilder::Build(syncer::DataTypeSet disabled_types,
                               syncer::SyncService* sync_service,
                               version_info::Channel channel) {
  std::vector<std::unique_ptr<syncer::DataTypeController>> controllers;

  auto add_controller =
      [&](std::unique_ptr<syncer::DataTypeController> controller) {
        if (controller) {
          controllers.push_back(std::move(controller));
        }
      };

  add_controller(CreateDeviceInfoDataTypeController());

  if (!disabled_types.Has(syncer::AUTOFILL)) {
    add_controller(CreateAutofillDataTypeController());
  }

  if (!disabled_types.Has(syncer::AUTOFILL_PROFILE)) {
    add_controller(CreateAutofillProfileDataTypeController());
  }

  if (!disabled_types.Has(syncer::CONTACT_INFO)) {
    add_controller(CreateContactInfoDataTypeController(sync_service));
  }

  if (!disabled_types.Has(syncer::AUTOFILL_WALLET_DATA)) {
    add_controller(CreateAutofillWalletDataTypeController(sync_service));
  }

  if (!disabled_types.Has(syncer::AUTOFILL_WALLET_METADATA) &&
      !disabled_types.Has(syncer::AUTOFILL_WALLET_DATA)) {
    add_controller(CreateAutofillWalletMetadataDataTypeController(sync_service));
  }

  if (!disabled_types.Has(syncer::AUTOFILL_WALLET_OFFER) &&
      !disabled_types.Has(syncer::AUTOFILL_WALLET_DATA)) {
    add_controller(CreateAutofillWalletOfferDataTypeController(sync_service));
  }

#if !BUILDFLAG(IS_IOS)
  if (!disabled_types.Has(syncer::AUTOFILL_WALLET_USAGE) &&
      !disabled_types.Has(syncer::AUTOFILL_WALLET_DATA)) {
    add_controller(CreateAutofillWalletUsageDataTypeController(sync_service));
  }
#endif

  if (!disabled_types.Has(syncer::AUTOFILL_WALLET_CREDENTIAL) &&
      !disabled_types.Has(syncer::AUTOFILL_WALLET_DATA)) {
    add_controller(
        CreateAutofillWalletCredentialDataTypeController(sync_service));
  }

  if (!disabled_types.Has(syncer::BOOKMARKS)) {
    add_controller(CreateBookmarksDataTypeController());
  }

  if (!disabled_types.Has(syncer::HISTORY)) {
    add_controller(CreateHistoryDataTypeController(sync_service));
  }

  if (!disabled_types.Has(syncer::HISTORY_DELETE_DIRECTIVES)) {
    add_controller(
        CreateHistoryDeleteDirectivesDataTypeController(sync_service, channel));
  }

  if (!disabled_types.Has(syncer::SESSIONS)) {
    add_controller(CreateSessionsDataTypeController(sync_service));
  }

  if (!disabled_types.Has(syncer::PASSWORDS)) {
    add_controller(CreatePasswordsDataTypeController());
  }

  if (!disabled_types.Has(syncer::INCOMING_PASSWORD_SHARING_INVITATION) &&
      !disabled_types.Has(syncer::PASSWORDS)) {
    add_controller(
        CreateIncomingPasswordSharingInvitationDataTypeController(sync_service));
  }

  if (!disabled_types.Has(syncer::OUTGOING_PASSWORD_SHARING_INVITATION) &&
      !disabled_types.Has(syncer::PASSWORDS)) {
    add_controller(
        CreateOutgoingPasswordSharingInvitationDataTypeController(sync_service));
  }

  if (!disabled_types.Has(syncer::PLUS_ADDRESS)) {
    add_controller(CreatePlusAddressDataTypeController());
  }

  if (!disabled_types.Has(syncer::PLUS_ADDRESS_SETTING)) {
    add_controller(CreatePlusAddressSettingDataTypeController());
  }

  if (!disabled_types.Has(syncer::PREFERENCES)) {
    add_controller(CreatePreferencesDataTypeController(channel));
  }

  if (!disabled_types.Has(syncer::PRIORITY_PREFERENCES)) {
    add_controller(CreatePriorityPreferencesDataTypeController(channel));
  }

  if (!disabled_types.Has(syncer::SAVED_TAB_GROUP)) {
    add_controller(CreateSavedTabGroupDataTypeController());
  }

  if (!disabled_types.Has(syncer::SHARED_TAB_GROUP_DATA)) {
    add_controller(CreateSharedTabGroupDataTypeController(sync_service));
  }

  if (!disabled_types.Has(syncer::SHARING_MESSAGE)) {
    add_controller(CreateSharingMessageDataTypeController());
  }

  if (!disabled_types.Has(syncer::READING_LIST)) {
    add_controller(CreateReadingListDataTypeController());
  }

  if (!disabled_types.Has(syncer::SEARCH_ENGINES)) {
    add_controller(CreateSearchEnginesDataTypeController(channel));
  }

  if (!disabled_types.Has(syncer::USER_EVENTS)) {
    add_controller(CreateUserEventsDataTypeController(sync_service));
  }

  if (!disabled_types.Has(syncer::SEND_TAB_TO_SELF)) {
    add_controller(CreateSendTabToSelfDataTypeController());
  }

  if (!disabled_types.Has(syncer::USER_CONSENTS)) {
    add_controller(CreateUserConsentsDataTypeController());
  }

  if (!disabled_types.Has(syncer::AUTOFILL_VALUABLE)) {
    add_controller(CreateAutofillValuableDataTypeController());
  }

  if (!disabled_types.Has(syncer::AUTOFILL_VALUABLE_METADATA)) {
    add_controller(CreateAutofillValuableMetadataDataTypeController());
  }

  if (!disabled_types.Has(syncer::ACCOUNT_SETTING)) {
    add_controller(CreateAccountSettingDataTypeController());
  }

  if (!disabled_types.Has(syncer::SHARED_TAB_GROUP_ACCOUNT_DATA)) {
    add_controller(CreateSharedTabGroupAccountDataTypeController(sync_service));
  }

  if (!disabled_types.Has(syncer::SHARED_COMMENT)) {
    add_controller(CreateSharedCommentDataTypeController());
  }

  if (!disabled_types.Has(syncer::AI_THREAD)) {
    add_controller(CreateAiThreadDataTypeController());
  }

  if (!disabled_types.Has(syncer::GEMINI_THREAD)) {
    add_controller(CreateGeminiThreadDataTypeController());
  }

  if (!disabled_types.Has(syncer::CONTEXTUAL_TASK)) {
    add_controller(CreateContextualTaskDataTypeController());
  }

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  if (!disabled_types.Has(syncer::SKILL)) {
    add_controller(CreateSkillDataTypeController());
  }
#endif

#if !BUILDFLAG(IS_ANDROID)
  if (!disabled_types.Has(syncer::WEBAUTHN_CREDENTIAL)) {
    add_controller(CreateWebauthnCredentialDataTypeController(sync_service));
  }
#endif

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  add_controller(CreateFamilyLinkSettingsDataTypeController(channel));
#endif

  if (!disabled_types.Has(syncer::COLLABORATION_GROUP)) {
    add_controller(CreateCollaborationGroupDataTypeController(sync_service));
  }

  return controllers;
}

std::unique_ptr<syncer::DataTypeController>
CommonControllerBuilder::CreateDeviceInfoDataTypeController() {
  return std::make_unique<DataTypeController>(
      syncer::DEVICE_INFO,
      /*delegate_for_full_sync_mode=*/
      std::make_unique<syncer::ForwardingDataTypeControllerDelegate>(
          device_info_sync_service_.value()->GetControllerDelegate().get()),
      /*delegate_for_transport_mode=*/
      std::make_unique<syncer::ForwardingDataTypeControllerDelegate>(
          device_info_sync_service_.value()->GetControllerDelegate().get()));
}

std::unique_ptr<syncer::DataTypeController>
CommonControllerBuilder::CreateAutofillDataTypeController() {
  if (!profile_autofill_web_data_service_.value()) {
    return nullptr;
  }
  return std::make_unique<DataTypeController>(
      syncer::AUTOFILL,
      std::make_unique<syncer::ProxyDataTypeControllerDelegate>(
          profile_autofill_web_data_service_.value()->GetDBTaskRunner(),
          base::BindRepeating(
              &AutocompleteDelegateFromDataService,
              base::RetainedRef(profile_autofill_web_data_service_.value()))),
      /*delegate_for_transport_mode=*/nullptr);
}

std::unique_ptr<syncer::DataTypeController>
CommonControllerBuilder::CreateAutofillProfileDataTypeController() {
  if (!profile_autofill_web_data_service_.value()) {
    return nullptr;
  }
  // Note: Transport mode is not and will not be supported - support for
  // addresses is provided via CONTACT_INFO instead.
  return std::make_unique<syncer::DataTypeController>(
      syncer::AUTOFILL_PROFILE,
      std::make_unique<syncer::ProxyDataTypeControllerDelegate>(
          profile_autofill_web_data_service_.value()->GetDBTaskRunner(),
          base::BindRepeating(
              &AutofillProfileDelegateFromDataService,
              base::RetainedRef(profile_autofill_web_data_service_.value()))),
      /*delegate_for_transport_mode=*/nullptr);
}

std::unique_ptr<syncer::DataTypeController>
CommonControllerBuilder::CreateContactInfoDataTypeController(
    syncer::SyncService* sync_service) {
  if (!profile_autofill_web_data_service_.value()) {
    return nullptr;
  }
  return std::make_unique<autofill::ContactInfoDataTypeController>(
      /*delegate_for_full_sync_mode=*/
      std::make_unique<syncer::ProxyDataTypeControllerDelegate>(
          profile_autofill_web_data_service_.value()->GetDBTaskRunner(),
          base::BindRepeating(
              &ContactInfoDelegateFromDataService,
              base::RetainedRef(profile_autofill_web_data_service_.value()))),
      /*delegate_for_transport_mode=*/
      std::make_unique<syncer::ProxyDataTypeControllerDelegate>(
          profile_autofill_web_data_service_.value()->GetDBTaskRunner(),
          base::BindRepeating(
              &ContactInfoDelegateFromDataService,
              base::RetainedRef(profile_autofill_web_data_service_.value()))),
      sync_service, identity_manager_.value(),
      std::make_unique<autofill::ContactInfoLocalDataBatchUploader>(
          address_data_manager_getter_));
}

std::unique_ptr<syncer::DataTypeController>
CommonControllerBuilder::CreateAutofillWalletDataTypeController(
    syncer::SyncService* sync_service) {
  if (!profile_autofill_web_data_service_.value()) {
    return nullptr;
  }
  return CreateWalletDataTypeController(
      syncer::AUTOFILL_WALLET_DATA,
      base::BindRepeating(&AutofillWalletDelegateFromDataService), sync_service,
      /*with_transport_mode_support=*/true);
}

std::unique_ptr<syncer::DataTypeController>
CommonControllerBuilder::CreateAutofillWalletMetadataDataTypeController(
    syncer::SyncService* sync_service) {
  if (!profile_autofill_web_data_service_.value()) {
    return nullptr;
  }
  return CreateWalletDataTypeController(
      syncer::AUTOFILL_WALLET_METADATA,
      base::BindRepeating(&AutofillWalletMetadataDelegateFromDataService),
      sync_service, /*with_transport_mode_support=*/
      syncer::IsReplaceSyncPromosWithSignInPromosEnabled());
}

std::unique_ptr<syncer::DataTypeController>
CommonControllerBuilder::CreateAutofillWalletOfferDataTypeController(
    syncer::SyncService* sync_service) {
  if (!profile_autofill_web_data_service_.value()) {
    return nullptr;
  }
  return CreateWalletDataTypeController(
      syncer::AUTOFILL_WALLET_OFFER,
      base::BindRepeating(&AutofillWalletOfferDelegateFromDataService),
      sync_service, /*with_transport_mode_support=*/
      syncer::IsReplaceSyncPromosWithSignInPromosEnabled());
}

#if !BUILDFLAG(IS_IOS)
std::unique_ptr<syncer::DataTypeController>
CommonControllerBuilder::CreateAutofillWalletUsageDataTypeController(
    syncer::SyncService* sync_service) {
  if (!profile_autofill_web_data_service_.value()) {
    return nullptr;
  }
  return CreateWalletDataTypeController(
      syncer::AUTOFILL_WALLET_USAGE,
      base::BindRepeating(&AutofillWalletUsageDataDelegateFromDataService),
      sync_service, /*with_transport_mode_support=*/true);
}
#endif

std::unique_ptr<syncer::DataTypeController>
CommonControllerBuilder::CreateAutofillWalletCredentialDataTypeController(
    syncer::SyncService* sync_service) {
  if (!profile_autofill_web_data_service_.value()) {
    return nullptr;
  }
  return CreateWalletDataTypeController(
      syncer::AUTOFILL_WALLET_CREDENTIAL,
      base::BindRepeating(&AutofillWalletCredentialDataDelegateFromDataService),
      sync_service, /*with_transport_mode_support=*/true);
}

std::unique_ptr<syncer::DataTypeController>
CommonControllerBuilder::CreateBookmarksDataTypeController() {
  if (!local_or_syncable_bookmark_sync_service_.value() ||
      !favicon_service_.value()) {
    return nullptr;
  }
  auto full_mode_delegate =
      std::make_unique<syncer::ForwardingDataTypeControllerDelegate>(
          local_or_syncable_bookmark_sync_service_.value()
              ->GetBookmarkSyncControllerDelegate(favicon_service_.value())
              .get());
  auto transport_mode_delegate =
      account_bookmark_sync_service_.value()
          ? std::make_unique<syncer::ForwardingDataTypeControllerDelegate>(
                account_bookmark_sync_service_.value()
                    ->GetBookmarkSyncControllerDelegate(
                        favicon_service_.value())
                    .get())
          : nullptr;
  return std::make_unique<sync_bookmarks::BookmarkDataTypeController>(
      std::move(full_mode_delegate), std::move(transport_mode_delegate),
      std::make_unique<sync_bookmarks::BookmarkLocalDataBatchUploader>(
          bookmark_model_.value(), pref_service_.value()));
}

std::unique_ptr<syncer::DataTypeController>
CommonControllerBuilder::CreateHistoryDataTypeController(
    syncer::SyncService* sync_service) {
  return std::make_unique<history::HistoryDataTypeController>(
      sync_service, history_service_.value(), pref_service_.value());
}

std::unique_ptr<syncer::DataTypeController>
CommonControllerBuilder::CreateHistoryDeleteDirectivesDataTypeController(
    syncer::SyncService* sync_service,
    version_info::Channel channel) {
  return std::make_unique<history::HistoryDeleteDirectivesDataTypeController>(
      base::BindRepeating(&syncer::ReportUnrecoverableError, channel),
      sync_service, data_type_store_service_.value(), history_service_.value(),
      pref_service_.value());
}

std::unique_ptr<syncer::DataTypeController>
CommonControllerBuilder::CreateSessionsDataTypeController(
    syncer::SyncService* sync_service) {
  syncer::DataTypeControllerDelegate* delegate =
      session_sync_service_.value()->GetControllerDelegate().get();
  auto full_sync_mode_delegate =
      std::make_unique<syncer::ForwardingDataTypeControllerDelegate>(delegate);
  auto transport_mode_delegate =
      syncer::IsReplaceSyncPromosWithSignInPromosEnabled()
          ? std::make_unique<syncer::ForwardingDataTypeControllerDelegate>(
                delegate)
          : nullptr;
  return std::make_unique<sync_sessions::SessionDataTypeController>(
      sync_service, pref_service_.value(), std::move(full_sync_mode_delegate),
      std::move(transport_mode_delegate));
}

std::unique_ptr<syncer::DataTypeController>
CommonControllerBuilder::CreatePasswordsDataTypeController() {
  // |profile_password_store_| can be null in tests.
  if (!profile_password_store_.value()) {
    return nullptr;
  }
  return std::make_unique<password_manager::PasswordDataTypeController>(
      profile_password_store_.value()->CreateSyncControllerDelegate(),
      account_password_store_.value()
          ? account_password_store_.value()->CreateSyncControllerDelegate()
          : nullptr,
      std::make_unique<password_manager::PasswordLocalDataBatchUploader>(
          profile_password_store_.value(), account_password_store_.value()));
}

std::unique_ptr<syncer::DataTypeController> CommonControllerBuilder::
    CreateIncomingPasswordSharingInvitationDataTypeController(
        syncer::SyncService* sync_service) {
  if (!profile_password_store_.value() || !password_receiver_service_.value()) {
    return nullptr;
  }
  return std::make_unique<
      password_manager::IncomingPasswordSharingInvitationDataTypeController>(
      sync_service, password_receiver_service_.value(), pref_service_.value());
}

std::unique_ptr<syncer::DataTypeController> CommonControllerBuilder::
    CreateOutgoingPasswordSharingInvitationDataTypeController(
        syncer::SyncService* sync_service) {
  if (!profile_password_store_.value() || !password_sender_service_.value()) {
    return nullptr;
  }
  return std::make_unique<
      password_manager::OutgoingPasswordSharingInvitationDataTypeController>(
      sync_service, password_sender_service_.value(), pref_service_.value());
}

std::unique_ptr<syncer::DataTypeController>
CommonControllerBuilder::CreatePlusAddressDataTypeController() {
  // `plus_address_webdata_service_` is null on iOS WebView.
  if (!plus_address_webdata_service_.value() ||
      !google_groups_manager_.value()) {
    return nullptr;
  }
  return std::make_unique<plus_addresses::PlusAddressDataTypeController>(
      syncer::PLUS_ADDRESS,
      /*delegate_for_full_sync_mode=*/
      plus_address_webdata_service_.value()->GetSyncControllerDelegate(),
      /*delegate_for_transport_mode=*/
      plus_address_webdata_service_.value()->GetSyncControllerDelegate(),
      google_groups_manager_.value());
}

std::unique_ptr<syncer::DataTypeController>
CommonControllerBuilder::CreatePlusAddressSettingDataTypeController() {
    // `plus_address_setting_service_` is null on iOS WebView.
  if (!plus_address_setting_service_.value() ||
      !google_groups_manager_.value()) {
    return nullptr;
  }
  return std::make_unique<plus_addresses::PlusAddressDataTypeController>(
      syncer::PLUS_ADDRESS_SETTING,
      /*delegate_for_full_sync_mode=*/
      plus_address_setting_service_.value()->GetSyncControllerDelegate(),
      /*delegate_for_transport_mode=*/
      plus_address_setting_service_.value()->GetSyncControllerDelegate(),
      google_groups_manager_.value());
}

std::unique_ptr<syncer::DataTypeController>
CommonControllerBuilder::CreatePreferencesDataTypeController(
    version_info::Channel channel) {
  return std::make_unique<SyncableServiceBasedDataTypeController>(
      syncer::PREFERENCES, data_type_store_service_.value()->GetStoreFactory(),
      SyncableServiceForPrefs(pref_service_syncable_.value(),
                              syncer::PREFERENCES),
      base::BindRepeating(&syncer::ReportUnrecoverableError, channel),
      ArePreferencesAllowedInTransportMode()
          ? SyncableServiceBasedDataTypeController::DelegateMode::
                kTransportModeWithSingleModel
          : SyncableServiceBasedDataTypeController::DelegateMode::
                kLegacyFullSyncModeOnly);
}

std::unique_ptr<syncer::DataTypeController>
CommonControllerBuilder::CreatePriorityPreferencesDataTypeController(
    version_info::Channel channel) {
  return std::make_unique<SyncableServiceBasedDataTypeController>(
      syncer::PRIORITY_PREFERENCES,
      data_type_store_service_.value()->GetStoreFactory(),
      SyncableServiceForPrefs(pref_service_syncable_.value(),
                              syncer::PRIORITY_PREFERENCES),
      base::BindRepeating(&syncer::ReportUnrecoverableError, channel),
      ArePreferencesAllowedInTransportMode()
          ? SyncableServiceBasedDataTypeController::DelegateMode::
                kTransportModeWithSingleModel
          : SyncableServiceBasedDataTypeController::DelegateMode::
                kLegacyFullSyncModeOnly);
}

std::unique_ptr<syncer::DataTypeController>
CommonControllerBuilder::CreateSavedTabGroupDataTypeController() {
  if (!tab_group_sync_service_.value()) {
    return nullptr;
  }
  syncer::DataTypeControllerDelegate* delegate =
      tab_group_sync_service_.value()
          ->GetSavedTabGroupControllerDelegate()
          .get();
  return std::make_unique<syncer::DataTypeController>(
      syncer::SAVED_TAB_GROUP,
      /*delegate_for_full_sync_mode=*/
      std::make_unique<syncer::ForwardingDataTypeControllerDelegate>(delegate),
      /*delegate_for_transport_mode=*/
      std::make_unique<syncer::ForwardingDataTypeControllerDelegate>(delegate));
}

std::unique_ptr<syncer::DataTypeController>
CommonControllerBuilder::CreateSharedTabGroupDataTypeController(
    syncer::SyncService* sync_service) {
  if (!tab_group_sync_service_.value() ||
      !data_sharing::features::IsDataSharingFunctionalityEnabled()) {
    return nullptr;
  }
  syncer::DataTypeControllerDelegate* delegate =
      tab_group_sync_service_.value()
          ->GetSharedTabGroupControllerDelegate()
          .get();
  return std::make_unique<collaboration::SharedTabGroupDataTypeController>(
      /*delegate_for_full_sync_mode=*/
      std::make_unique<syncer::ForwardingDataTypeControllerDelegate>(delegate),
      /*delegate_for_transport_mode=*/
      std::make_unique<syncer::ForwardingDataTypeControllerDelegate>(delegate),
      sync_service, collaboration_service_.value());
}

std::unique_ptr<syncer::DataTypeController>
CommonControllerBuilder::CreateSharingMessageDataTypeController() {
  if (!sharing_message_bridge_.value()) {
    return nullptr;
  }
  syncer::DataTypeControllerDelegate* sharing_message_delegate =
      sharing_message_bridge_.value()->GetControllerDelegate().get();
  return std::make_unique<SharingMessageDataTypeController>(
      /*delegate_for_full_sync_mode=*/
      std::make_unique<syncer::ForwardingDataTypeControllerDelegate>(
          sharing_message_delegate),
      /*delegate_for_transport_mode=*/
      std::make_unique<syncer::ForwardingDataTypeControllerDelegate>(
          sharing_message_delegate));
}

std::unique_ptr<syncer::DataTypeController>
CommonControllerBuilder::CreateReadingListDataTypeController() {
  syncer::DataTypeControllerDelegate* delegate_for_transport_mode =
      dual_reading_list_model_.value()
          ->GetSyncControllerDelegateForTransportMode()
          .get();
  return std::make_unique<DataTypeController>(
      syncer::READING_LIST,
      /*delegate_for_full_sync_mode=*/
      std::make_unique<syncer::ForwardingDataTypeControllerDelegate>(
          dual_reading_list_model_.value()->GetSyncControllerDelegate().get()),
      /*delegate_for_transport_mode=*/
      delegate_for_transport_mode
          ? std::make_unique<syncer::ForwardingDataTypeControllerDelegate>(
                delegate_for_transport_mode)
          : nullptr,
      std::make_unique<reading_list::ReadingListLocalDataBatchUploader>(
          dual_reading_list_model_.value()));
}

std::unique_ptr<syncer::DataTypeController>
CommonControllerBuilder::CreateSearchEnginesDataTypeController(
    version_info::Channel channel) {
  if (!template_url_service_.value()) {
    return nullptr;
  }
  return std::make_unique<syncer::SyncableServiceBasedDataTypeController>(
      syncer::SEARCH_ENGINES,
      data_type_store_service_.value()->GetStoreFactory(),
      template_url_service_.value()->AsWeakPtr(),
      base::BindRepeating(&syncer::ReportUnrecoverableError, channel),
      base::FeatureList::IsEnabled(
          syncer::kSeparateLocalAndAccountSearchEngines)
          ? syncer::SyncableServiceBasedDataTypeController::DelegateMode::
                kTransportModeWithSingleModel
          : syncer::SyncableServiceBasedDataTypeController::DelegateMode::
                kLegacyFullSyncModeOnly);
}

std::unique_ptr<syncer::DataTypeController>
CommonControllerBuilder::CreateUserEventsDataTypeController(
    syncer::SyncService* sync_service) {
  syncer::DataTypeControllerDelegate* delegate =
      user_event_service_.value()->GetControllerDelegate().get();
  return std::make_unique<syncer::UserEventDataTypeController>(
      sync_service,
      /*delegate_for_full_sync_mode=*/
      std::make_unique<syncer::ForwardingDataTypeControllerDelegate>(delegate),
      /*delegate_for_transport_mode=*/
      syncer::IsReplaceSyncPromosWithSignInPromosEnabled()
          ? std::make_unique<syncer::ForwardingDataTypeControllerDelegate>(
                delegate)
          : nullptr);
}

std::unique_ptr<syncer::DataTypeController>
CommonControllerBuilder::CreateSendTabToSelfDataTypeController() {
  syncer::DataTypeControllerDelegate* delegate =
      send_tab_to_self_sync_service_.value()->GetControllerDelegate().get();
  return std::make_unique<send_tab_to_self::SendTabToSelfDataTypeController>(
      /*delegate_for_full_sync_mode=*/
      std::make_unique<syncer::ForwardingDataTypeControllerDelegate>(delegate),
      /*delegate_for_transport_mode=*/
      std::make_unique<syncer::ForwardingDataTypeControllerDelegate>(delegate));
}

std::unique_ptr<syncer::DataTypeController>
CommonControllerBuilder::CreateUserConsentsDataTypeController() {
  syncer::DataTypeControllerDelegate* delegate =
      consent_auditor_.value()->GetControllerDelegate().get();
  return std::make_unique<DataTypeController>(
      syncer::USER_CONSENTS,
      /*delegate_for_full_sync_mode=*/
      std::make_unique<syncer::ForwardingDataTypeControllerDelegate>(delegate),
      /*delegate_for_transport_mode=*/
      std::make_unique<syncer::ForwardingDataTypeControllerDelegate>(delegate));
}

std::unique_ptr<syncer::DataTypeController>
CommonControllerBuilder::CreateAutofillValuableDataTypeController() {
#if BUILDFLAG(IS_IOS)
  if (!base::FeatureList::IsEnabled(syncer::kSyncAutofillValuable)) {
    return nullptr;
  }
#endif
  if (!profile_autofill_web_data_service_.value()) {
    return nullptr;
  }
  return std::make_unique<autofill::AutofillValuableDataTypeController>(
      syncer::AUTOFILL_VALUABLE,
      std::make_unique<syncer::ProxyDataTypeControllerDelegate>(
          profile_autofill_web_data_service_.value()->GetDBTaskRunner(),
          base::BindRepeating(
              &AutofillValuableDelegateFromDataService,
              base::RetainedRef(profile_autofill_web_data_service_.value()))),
      std::make_unique<syncer::ProxyDataTypeControllerDelegate>(
          profile_autofill_web_data_service_.value()->GetDBTaskRunner(),
          base::BindRepeating(
              &AutofillValuableDelegateFromDataService,
              base::RetainedRef(profile_autofill_web_data_service_.value()))));
}

std::unique_ptr<syncer::DataTypeController>
CommonControllerBuilder::CreateAutofillValuableMetadataDataTypeController() {
  if (!profile_autofill_web_data_service_.value() ||
      !base::FeatureList::IsEnabled(syncer::kSyncAutofillValuableMetadata)) {
    return nullptr;
  }
  return std::make_unique<autofill::AutofillValuableDataTypeController>(
      syncer::AUTOFILL_VALUABLE_METADATA,
      std::make_unique<syncer::ProxyDataTypeControllerDelegate>(
          profile_autofill_web_data_service_.value()->GetDBTaskRunner(),
          base::BindRepeating(
              &AutofillValuableMetadataDelegateFromDataService,
              base::RetainedRef(profile_autofill_web_data_service_.value()))),
      std::make_unique<syncer::ProxyDataTypeControllerDelegate>(
          profile_autofill_web_data_service_.value()->GetDBTaskRunner(),
          base::BindRepeating(
              &AutofillValuableMetadataDelegateFromDataService,
              base::RetainedRef(profile_autofill_web_data_service_.value()))));
}

std::unique_ptr<syncer::DataTypeController>
CommonControllerBuilder::CreateAccountSettingDataTypeController() {
  if (!account_setting_service_.value() ||
      !base::FeatureList::IsEnabled(syncer::kSyncAccountSettings)) {
    return nullptr;
  }
  return std::make_unique<DataTypeController>(
      syncer::ACCOUNT_SETTING,
      /*delegate_for_full_sync_mode=*/
      account_setting_service_.value()->GetSyncControllerDelegate(),
      /*delegate_for_transport_mode=*/
      account_setting_service_.value()->GetSyncControllerDelegate());
}

std::unique_ptr<syncer::DataTypeController>
CommonControllerBuilder::CreateSharedTabGroupAccountDataTypeController(
    syncer::SyncService* sync_service) {
  if (!base::FeatureList::IsEnabled(syncer::kSyncSharedTabGroupAccountData) ||
      !data_sharing::features::IsDataSharingFunctionalityEnabled()) {
    return nullptr;
  }
  syncer::DataTypeControllerDelegate* delegate = nullptr;
  if (personal_collaboration_data_service_.value()) {
    delegate = personal_collaboration_data_service_.value()
                   ->GetControllerDelegate()
                   .get();
  } else if (tab_group_sync_service_.value()) {
    delegate = tab_group_sync_service_.value()
                   ->GetSharedTabGroupAccountControllerDelegate()
                   .get();
  }
  if (!delegate) {
    return nullptr;
  }
  return std::make_unique<
      collaboration::SharedTabGroupAccountDataTypeController>(
      /*delegate_for_full_sync_mode=*/
      std::make_unique<syncer::ForwardingDataTypeControllerDelegate>(delegate),
      /*delegate_for_transport_mode=*/
      std::make_unique<syncer::ForwardingDataTypeControllerDelegate>(delegate),
      sync_service, collaboration_service_.value());
}

std::unique_ptr<syncer::DataTypeController>
CommonControllerBuilder::CreateSharedCommentDataTypeController() {
  if (!base::FeatureList::IsEnabled(syncer::kSyncSharedComment)) {
    return nullptr;
  }

  // TODO(crbug.com/433556051): In a future CL, register the type, i.e.
  // instantiate the DataTypeController. There is more than one way to go
  // about it, but one option is:
  // - Create a trivial implementation of DataTypeSyncBridge which lives in
  //   your feature's directory. It should have synchronous access to your
  //   data model (e.g. DualReadingListModel) and be (indirectly) owned by a
  //   CoolKeyedService (often the model itself).
  // - Expose CoolKeyedService::GetControllerDelegate() which calls
  //   bridge->change_processor()->GetControllerDelegate().
  // - Inject CoolKeyedService in this class and call GetControllerDelegate()
  //   on it to create the DataTypeController.
  // In following CLs implement the bridge and keep adding unit tests.
  return nullptr;
}

std::unique_ptr<syncer::DataTypeController>
CommonControllerBuilder::CreateAiThreadDataTypeController() {
  if (!base::FeatureList::IsEnabled(syncer::kSyncAIThread) ||
      !contextual_tasks_service_.value() || !aim_eligibility_service_.value()) {
    return nullptr;
  }
  syncer::DataTypeControllerDelegate* delegate =
      contextual_tasks_service_.value()->GetAiThreadControllerDelegate().get();
  if (!delegate) {
    return nullptr;
  }
  return std::make_unique<contextual_tasks::AIThreadDataTypeController>(
      aim_eligibility_service_.value(),
      /*delegate_for_full_sync_mode= */
      std::make_unique<syncer::ForwardingDataTypeControllerDelegate>(delegate),
      /*delegate_for_transport_mode= */
      std::make_unique<syncer::ForwardingDataTypeControllerDelegate>(delegate));
}

std::unique_ptr<syncer::DataTypeController>
CommonControllerBuilder::CreateGeminiThreadDataTypeController() {
  if (!base::FeatureList::IsEnabled(syncer::kSyncGeminiThread) ||
      !contextual_tasks_service_.value()) {
    return nullptr;
  }
  syncer::DataTypeControllerDelegate* delegate =
      contextual_tasks_service_.value()
          ->GetGeminiThreadControllerDelegate()
          .get();
  if (!delegate) {
    return nullptr;
  }
  return std::make_unique<contextual_tasks::GeminiThreadDataTypeController>(
      /*contextual_tasks_service=*/contextual_tasks_service_.value(),
      /*delegate_for_full_sync_mode= */
      std::make_unique<syncer::ForwardingDataTypeControllerDelegate>(delegate),
      /*delegate_for_transport_mode= */
      std::make_unique<syncer::ForwardingDataTypeControllerDelegate>(delegate));
}

std::unique_ptr<syncer::DataTypeController>
CommonControllerBuilder::CreateContextualTaskDataTypeController() {
  if (!base::FeatureList::IsEnabled(syncer::kSyncContextualTask)) {
    return nullptr;
  }
  // TODO(crbug.com/445840788): In CL #4, register the type, i.e. instantiate
  // the DataTypeController. There is more than one way to go about it,
  // but one option is:
  // - Create a trivial implementation of DataTypeSyncBridge which lives in
  //   your feature's directory. It should have synchronous access to your
  //   data model (e.g. DualReadingListModel) and be (indirectly) owned by a
  //   CoolKeyedService (often the model itself).
  // - Expose CoolKeyedService::GetControllerDelegate() which calls
  //   bridge->change_processor()->GetControllerDelegate().
  // - Inject CoolKeyedService in this class and call GetControllerDelegate()
  //   on it to create the DataTypeController.
  // In CLs #5, #6, ..., implement the bridge and keep adding unit tests.
  return nullptr;
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
std::unique_ptr<syncer::DataTypeController>
CommonControllerBuilder::CreateSkillDataTypeController() {
  if (!base::FeatureList::IsEnabled(features::kSkillsEnabled) ||
      !skills_service_.value()) {
    return nullptr;
  }
  syncer::DataTypeControllerDelegate* delegate =
      skills_service_.value()->GetControllerDelegate().get();
  if (!delegate) {
    return nullptr;
  }
  return std::make_unique<syncer::DataTypeController>(
      syncer::SKILL,
      /*delegate_for_full_sync_mode=*/
      std::make_unique<syncer::ForwardingDataTypeControllerDelegate>(delegate),
      /*delegate_for_transport_mode=*/
      std::make_unique<syncer::ForwardingDataTypeControllerDelegate>(delegate));
}
#endif

#if !BUILDFLAG(IS_ANDROID)
std::unique_ptr<syncer::DataTypeController>
CommonControllerBuilder::CreateWebauthnCredentialDataTypeController(
    syncer::SyncService* sync_service) {
  if (!passkey_model_.value()) {
    return nullptr;
  }
  syncer::DataTypeControllerDelegate* delegate =
      passkey_model_.value()->GetDataTypeControllerDelegate().get();
  return std::make_unique<webauthn::PasskeyDataTypeController>(
      sync_service,
      /*delegate_for_full_sync_mode=*/
      std::make_unique<syncer::ForwardingDataTypeControllerDelegate>(delegate),
      /*delegate_for_transport_mode=*/
      std::make_unique<syncer::ForwardingDataTypeControllerDelegate>(delegate));
}
#endif

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
std::unique_ptr<syncer::DataTypeController>
CommonControllerBuilder::CreateFamilyLinkSettingsDataTypeController(
    version_info::Channel channel) {
  if (!family_link_settings_service_.value()) {
    return nullptr;
  }
  return std::make_unique<FamilyLinkSettingsDataTypeController>(
      base::BindRepeating(&syncer::ReportUnrecoverableError, channel),
      data_type_store_service_.value()->GetStoreFactory(),
      family_link_settings_service_.value()->AsWeakPtr(),
      pref_service_.value());
}
#endif

std::unique_ptr<syncer::DataTypeController>
CommonControllerBuilder::CreateCollaborationGroupDataTypeController(
    syncer::SyncService* sync_service) {
  if (!data_sharing_service_.value() ||
      !data_sharing::features::IsDataSharingFunctionalityEnabled()) {
    return nullptr;
  }
  syncer::DataTypeControllerDelegate* delegate =
      data_sharing_service_.value()
          ->GetCollaborationGroupControllerDelegate()
          .get();
  return std::make_unique<collaboration::CollaborationGroupDataTypeController>(
      /*delegate_for_full_sync_mode=*/
      std::make_unique<syncer::ForwardingDataTypeControllerDelegate>(delegate),
      /*delegate_for_transport_mode=*/
      std::make_unique<syncer::ForwardingDataTypeControllerDelegate>(delegate),
      sync_service, collaboration_service_.value());
}

std::unique_ptr<DataTypeController>
CommonControllerBuilder::CreateWalletDataTypeController(
    syncer::DataType type,
    const base::RepeatingCallback<
        base::WeakPtr<syncer::DataTypeControllerDelegate>(
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
      std::make_unique<syncer::ProxyDataTypeControllerDelegate>(
          profile_autofill_web_data_service_.value()->GetDBTaskRunner(),
          base::BindRepeating(
              delegate_from_web_data,
              base::RetainedRef(profile_autofill_web_data_service_.value())));
  auto delegate_for_transport_mode =
      with_transport_mode_support
          ? std::make_unique<syncer::ProxyDataTypeControllerDelegate>(
                account_autofill_web_data_service_.value()->GetDBTaskRunner(),
                base::BindRepeating(
                    delegate_from_web_data,
                    base::RetainedRef(
                        account_autofill_web_data_service_.value())))
          : nullptr;
  return std::make_unique<AutofillWalletDataTypeController>(
      type, std::move(delegate_for_full_sync_mode),
      std::move(delegate_for_transport_mode), pref_service_.value(),
      sync_service);
}

}  // namespace browser_sync
