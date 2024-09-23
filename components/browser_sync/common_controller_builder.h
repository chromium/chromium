// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_SYNC_COMMON_CONTROLLER_BUILDER_H_
#define COMPONENTS_BROWSER_SYNC_COMMON_CONTROLLER_BUILDER_H_

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "components/supervised_user/core/common/buildflags.h"
#include "components/sync/base/data_type.h"

class GoogleGroupsManager;
class PrefService;
class SharingMessageBridge;
class TemplateURLService;

namespace autofill {
class AutofillWebDataService;
}  // namespace autofill

namespace bookmarks {
class BookmarkModel;
}  // namespace bookmarks

namespace commerce {
class ProductSpecificationsService;
}  // namespace commerce

namespace consent_auditor {
class ConsentAuditor;
}  // namespace consent_auditor

namespace data_sharing {
class DataSharingService;
}  // namespace data_sharing

namespace favicon {
class FaviconService;
}  // namespace favicon

namespace history {
class HistoryService;
}  // namespace history

namespace password_manager {
class PasswordReceiverService;
class PasswordSenderService;
class PasswordStoreInterface;
}  // namespace password_manager

namespace plus_addresses {
class PlusAddressSettingService;
class PlusAddressWebDataService;
}  // namespace plus_addresses

namespace power_bookmarks {
class PowerBookmarkService;
}  // namespace power_bookmarks

namespace reading_list {
class DualReadingListModel;
}  // namespace reading_list

namespace send_tab_to_self {
class SendTabToSelfSyncService;
}  // namespace send_tab_to_self

namespace signin {
class IdentityManager;
}  // namespace signin

namespace supervised_user {
class SupervisedUserSettingsService;
}  // namespace supervised_user

namespace sync_bookmarks {
class BookmarkSyncService;
}  // namespace sync_bookmarks

namespace sync_preferences {
class PrefServiceSyncable;
}  // namespace sync_preferences

namespace sync_sessions {
class SessionSyncService;
}  // namespace sync_sessions

namespace syncer {
class DeviceInfoSyncService;
class DataTypeController;
class DataTypeControllerDelegate;
class DataTypeStoreService;
class SyncService;
class UserEventService;
}  // namespace syncer

namespace tab_groups {
class TabGroupSyncService;
}  // namespace tab_groups

namespace version_info {
enum class Channel;
}  // namespace version_info

namespace webauthn {
class PasskeyModel;
}  // namespace webauthn

namespace browser_sync {

// Class responsible for instantiating sync controllers (DataTypeController)
// for most sync datatypes / features. This includes datatypes that are
// supported or planned on all major platforms. Users of this class need to
// inject dependencies by invoking all setters (more on this below) and finally
// invoke `Build()` to instantiate controllers.
class CommonControllerBuilder {
 public:
  CommonControllerBuilder();
  ~CommonControllerBuilder();

  // Setters to inject dependencies. Each of these setters must be invoked
  // before invoking `Build()`. In some cases it is allowed to inject nullptr.
  void SetAutofillWebDataService(
      const scoped_refptr<base::SequencedTaskRunner>& ui_thread,
      const scoped_refptr<autofill::AutofillWebDataService>&
          web_data_service_on_disk,
      const scoped_refptr<autofill::AutofillWebDataService>&
          web_data_service_in_memory);
  void SetBookmarkModel(bookmarks::BookmarkModel* bookmark_model);
  void SetBookmarkSyncService(
      sync_bookmarks::BookmarkSyncService*
          local_or_syncable_bookmark_sync_service,
      sync_bookmarks::BookmarkSyncService* account_bookmark_sync_service);
  void SetConsentAuditor(consent_auditor::ConsentAuditor* consent_auditor);
  void SetDataSharingService(
      data_sharing::DataSharingService* data_sharing_service);
  void SetDeviceInfoSyncService(
      syncer::DeviceInfoSyncService* device_info_sync_service);
  void SetFaviconService(favicon::FaviconService* favicon_service);
  void SetGoogleGroupsManager(GoogleGroupsManager* google_groups_manager);
  void SetHistoryService(history::HistoryService* history_service);
  void SetIdentityManager(signin::IdentityManager* identity_manager);
  void SetDataTypeStoreService(
      syncer::DataTypeStoreService* data_type_store_service);

#if !BUILDFLAG(IS_ANDROID)
  void SetPasskeyModel(webauthn::PasskeyModel* passkey_model);
#endif  // !BUILDFLAG(IS_ANDROID)

  void SetPasswordReceiverService(
      password_manager::PasswordReceiverService* password_receiver_service);
  void SetPasswordSenderService(
      password_manager::PasswordSenderService* password_sender_service);
  void SetPasswordStore(
      const scoped_refptr<password_manager::PasswordStoreInterface>&
          profile_password_store,
      const scoped_refptr<password_manager::PasswordStoreInterface>&
          account_password_store);
  void SetPlusAddressServices(
      plus_addresses::PlusAddressSettingService* plus_address_setting_service,
      const scoped_refptr<plus_addresses::PlusAddressWebDataService>&
          plus_address_webdata_service);
  void SetPowerBookmarkService(
      power_bookmarks::PowerBookmarkService* power_bookmark_service);
  void SetPrefService(PrefService* pref_service);
  void SetPrefServiceSyncable(
      sync_preferences::PrefServiceSyncable* pref_service_syncable);
  void SetProductSpecificationsService(
      commerce::ProductSpecificationsService* product_specifications_service);
  void SetDualReadingListModel(
      reading_list::DualReadingListModel* dual_reading_list_model);
  void SetSendTabToSelfSyncService(send_tab_to_self::SendTabToSelfSyncService*
                                       send_tab_to_self_sync_service);
  void SetSessionSyncService(
      sync_sessions::SessionSyncService* session_sync_service);
  void SetSharingMessageBridge(SharingMessageBridge* sharing_message_bridge);

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  void SetSupervisedUserSettingsService(
      supervised_user::SupervisedUserSettingsService*
          supervised_user_settings_service);
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)

  void SetTabGroupSyncService(
      tab_groups::TabGroupSyncService* tab_group_sync_service);
  void SetTemplateURLService(TemplateURLService* template_url_service);
  void SetUserEventService(syncer::UserEventService* user_event_service);

  // Actually builds the controllers. All setters above must have been called
  // beforehand (null may or may not be allowed).
  std::vector<std::unique_ptr<syncer::DataTypeController>> Build(
      syncer::DataTypeSet disabled_types,
      syncer::SyncService* sync_service,
      version_info::Channel channel);

 private:
  // Minimalistic fork of std::optional that enforces via CHECK that it has a
  // value when accessing it.
  template <typename Ptr>
  class SafeOptional {
   public:
    SafeOptional() = default;
    ~SafeOptional() = default;

    void Set(Ptr ptr) {
      CHECK(!ptr_.has_value());
      ptr_.emplace(std::move(ptr));
    }

    // Set() must have been called before.
    Ptr value() const {
      CHECK(ptr_.has_value());
      return ptr_.value();
    }

   private:
    std::optional<Ptr> ptr_;
  };

  // Factory function for DataTypeController instances for wallet-related
  // datatypes, which live in `db_thread_` and have a delegate accessible via
  // AutofillWebDataService.
  // If `with_transport_mode_support` is true, the controller will support
  // transport mode, implemented via an independent AutofillWebDataService,
  // namely `web_data_service_in_memory_`.
  std::unique_ptr<syncer::DataTypeController> CreateWalletDataTypeController(
      syncer::DataType type,
      const base::RepeatingCallback<
          base::WeakPtr<syncer::DataTypeControllerDelegate>(
              autofill::AutofillWebDataService*)>& delegate_from_web_data,
      syncer::SyncService* sync_service,
      bool with_transport_mode_support);

  // For all above, nullopt indicates the corresponding setter wasn't invoked.
  // nullptr indicates the setter was invoked with nullptr.
  SafeOptional<raw_ptr<signin::IdentityManager>> identity_manager_;
  SafeOptional<raw_ptr<consent_auditor::ConsentAuditor>> consent_auditor_;
  SafeOptional<raw_ptr<syncer::DeviceInfoSyncService>>
      device_info_sync_service_;
  SafeOptional<raw_ptr<favicon::FaviconService>> favicon_service_;
  SafeOptional<raw_ptr<GoogleGroupsManager>> google_groups_manager_;
  SafeOptional<raw_ptr<history::HistoryService>> history_service_;
  SafeOptional<raw_ptr<syncer::DataTypeStoreService>> data_type_store_service_;
  SafeOptional<raw_ptr<webauthn::PasskeyModel>> passkey_model_;
  SafeOptional<raw_ptr<password_manager::PasswordReceiverService>>
      password_receiver_service_;
  SafeOptional<raw_ptr<password_manager::PasswordSenderService>>
      password_sender_service_;
  SafeOptional<raw_ptr<PrefService>> pref_service_;
  SafeOptional<raw_ptr<sync_preferences::PrefServiceSyncable>>
      pref_service_syncable_;
  SafeOptional<raw_ptr<sync_sessions::SessionSyncService>>
      session_sync_service_;
  SafeOptional<raw_ptr<reading_list::DualReadingListModel>>
      dual_reading_list_model_;
  SafeOptional<raw_ptr<send_tab_to_self::SendTabToSelfSyncService>>
      send_tab_to_self_sync_service_;
  SafeOptional<raw_ptr<syncer::UserEventService>> user_event_service_;
  SafeOptional<scoped_refptr<base::SequencedTaskRunner>>
      autofill_web_data_ui_thread_;
  SafeOptional<scoped_refptr<autofill::AutofillWebDataService>>
      autofill_web_data_service_on_disk_;
  SafeOptional<scoped_refptr<autofill::AutofillWebDataService>>
      autofill_web_data_service_in_memory_;
  SafeOptional<scoped_refptr<password_manager::PasswordStoreInterface>>
      profile_password_store_;
  SafeOptional<scoped_refptr<password_manager::PasswordStoreInterface>>
      account_password_store_;
  SafeOptional<raw_ptr<sync_bookmarks::BookmarkSyncService>>
      local_or_syncable_bookmark_sync_service_;
  SafeOptional<raw_ptr<sync_bookmarks::BookmarkSyncService>>
      account_bookmark_sync_service_;
  SafeOptional<raw_ptr<bookmarks::BookmarkModel>> bookmark_model_;
  SafeOptional<raw_ptr<power_bookmarks::PowerBookmarkService>>
      power_bookmark_service_;
  SafeOptional<raw_ptr<supervised_user::SupervisedUserSettingsService>>
      supervised_user_settings_service_;
  SafeOptional<raw_ptr<plus_addresses::PlusAddressSettingService>>
      plus_address_setting_service_;
  SafeOptional<scoped_refptr<plus_addresses::PlusAddressWebDataService>>
      plus_address_webdata_service_;
  SafeOptional<raw_ptr<commerce::ProductSpecificationsService>>
      product_specifications_service_;
  SafeOptional<raw_ptr<data_sharing::DataSharingService>> data_sharing_service_;
  SafeOptional<raw_ptr<SharingMessageBridge>> sharing_message_bridge_;
  SafeOptional<raw_ptr<tab_groups::TabGroupSyncService>>
      tab_group_sync_service_;
  SafeOptional<raw_ptr<TemplateURLService>> template_url_service_;
};

}  // namespace browser_sync

#endif  // COMPONENTS_BROWSER_SYNC_COMMON_CONTROLLER_BUILDER_H_
