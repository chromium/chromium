// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_SETTINGS_SERVICE_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_SETTINGS_SERVICE_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_store.h"
#include "components/supervised_user/core/common/supervised_users.h"
#include "components/sync/model/syncable_service.h"
#include "url/gurl.h"

class PersistentPrefStore;

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace supervised_user {

// This class syncs supervised user settings from a server, which are mapped to
// preferences. The downloaded settings are persisted in a PrefStore (which is
// not directly hooked up to the PrefService; it's just used internally).
// Settings are key-value pairs, where the key uniquely identifies the setting.
// The value is a string containing a JSON serialization of an arbitrary value,
// which is the value of the setting.
//
// There are two kinds of settings handled by this class: Atomic and split
// settings.
// Atomic settings consist of a single key (which will be mapped to a pref key)
// and a single (arbitrary) value.
// Split settings encode a dictionary value and are stored as multiple Sync
// items, one for each dictionary entry. The key for each of these Sync items
// is the key of the split setting, followed by a separator (':') and the key
// for the dictionary entry. The value of the Sync item is the value of the
// dictionary entry.
//
// As an example, a split setting with key "Moose" and value
//   {
//     "foo": "bar",
//     "baz": "blurp"
//   }
// would be encoded as two sync items, one with key "Moose:foo" and value "bar",
// and one with key "Moose:baz" and value "blurp".
class SupervisedUserSettingsService : public KeyedService,
                                      public syncer::SyncableService,
                                      public PrefStore::Observer {
 public:
  // A callback whose first parameter is a dictionary containing all supervised
  // user settings. If the dictionary is empty, it means that the service is
  // inactive, i.e. the user is not supervised.
  using SettingsCallbackType = void(const base::Value::Dict&);
  using SettingsCallback = base::RepeatingCallback<SettingsCallbackType>;
  using SettingsCallbackList =
      base::RepeatingCallbackList<SettingsCallbackType>;

  // Called when a new host is remotely approved for this supervised user. The
  // first param is newly approved host, which might be a pattern containing
  // wildcards (e.g. "*.google.*"").
  using WebsiteApprovalCallbackType = void(const std::string& hostname);
  using WebsiteApprovalCallback =
      base::RepeatingCallback<WebsiteApprovalCallbackType>;
  using WebsiteApprovalCallbackList =
      base::RepeatingCallbackList<WebsiteApprovalCallbackType>;

  using ShutdownCallbackType = void();
  using ShutdownCallback = base::RepeatingCallback<ShutdownCallbackType>;
  using ShutdownCallbackList =
      base::RepeatingCallbackList<ShutdownCallbackType>;

  SupervisedUserSettingsService();

  SupervisedUserSettingsService(const SupervisedUserSettingsService&) = delete;
  SupervisedUserSettingsService& operator=(
      const SupervisedUserSettingsService&) = delete;

  ~SupervisedUserSettingsService() override;

  // Initializes the service by loading its settings from a file underneath the
  // |profile_path|. File I/O will be serialized via the
  // |sequenced_task_runner|. If |load_synchronously| is true, the settings will
  // be loaded synchronously, otherwise asynchronously.
  void Init(base::FilePath profile_path,
            scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner,
            bool load_synchronously);

  // Initializes the service by loading its settings from the |pref_store|.
  // Use this method in tests to inject a different PrefStore than the
  // default one.
  void Init(scoped_refptr<PersistentPrefStore> pref_store);

  // Adds a callback to be called when supervised user settings are initially
  // available, or when they change.
  [[nodiscard]] base::CallbackListSubscription SubscribeForSettingsChange(
      const SettingsCallback& callback);

  // Subscribes to be notified when a new website is remotely approved for this
  // user.
  [[nodiscard]] base::CallbackListSubscription SubscribeForNewWebsiteApproval(
      const WebsiteApprovalCallback& callback);

  // Records that a website has been locally approved for this user.
  //
  // This handles updating local and remote state for this setting, and
  // notifying observers.
  virtual void RecordLocalWebsiteApproval(const std::string& host);

  // Subscribe for a notification when the keyed service is shut down. The
  // subscription can be destroyed to unsubscribe.
  base::CallbackListSubscription SubscribeForShutdown(
      const ShutdownCallback& callback);

  // Activates/deactivates the service. This is called by the
  // SupervisedUserService when it is (de)activated.
  void SetActive(bool active);

  // Whether supervised user settings are available.
  bool IsReady() const;

  // Clears all supervised user settings and items.
  void Clear();

  // Constructs a key for a split supervised user setting from a prefix and a
  // variable key.
  static std::string MakeSplitSettingKey(const std::string& prefix,
                                         const std::string& key);

  // Sets an item locally and uploads it to the Sync server.
  //
  // This handles notifying subscribers of the change.
  //
  // This may be called regardless of whether the sync server has completed
  // initialization; in either case the local changes will be handled
  // immediately.
  void SaveItem(const std::string& key, base::Value value);

  // Sets the setting with the given `key` to `value`.
  void SetLocalSetting(std::string_view key, base::Value value);
  void SetLocalSetting(std::string_view key, base::Value::Dict dict);

  // Removes the setting for `key`.
  void RemoveLocalSetting(std::string_view key);

  // Public for testing.
  static syncer::SyncData CreateSyncDataForSetting(const std::string& name,
                                                   const base::Value& value);

  // KeyedService implementation:
  void Shutdown() override;

  // SyncableService implementation:
  void WaitUntilReadyToSync(base::OnceClosure done) override;
  std::optional<syncer::ModelError> MergeDataAndStartSyncing(
      syncer::DataType type,
      const syncer::SyncDataList& initial_sync_data,
      std::unique_ptr<syncer::SyncChangeProcessor> sync_processor) override;
  void StopSyncing(syncer::DataType type) override;
  syncer::SyncDataList GetAllSyncDataForTesting(syncer::DataType type) const;
  std::optional<syncer::ModelError> ProcessSyncChanges(
      const base::Location& from_here,
      const syncer::SyncChangeList& change_list) override;
  base::WeakPtr<SyncableService> AsWeakPtr() override;

  // PrefStore::Observer implementation:
  void OnInitializationCompleted(bool success) override;

  bool IsCustomPassphraseAllowed() const;

  const base::Value::Dict& LocalSettingsForTest() const;

  // Returns the dictionary where a given Sync item should be stored, depending
  // on whether the supervised user setting is atomic or split. In case of a
  // split setting, the split setting prefix of |key| is removed, so that |key|
  // can be used to update the returned dictionary.
  base::Value::Dict* GetDictionaryAndSplitKey(std::string* key) const;

 private:
  base::Value::Dict* GetOrCreateDictionary(const std::string& key) const;
  base::Value::Dict* GetAtomicSettings() const;
  base::Value::Dict* GetSplitSettings() const;
  base::Value::Dict* GetQueuedItems() const;

  // Returns a dictionary with all supervised user settings if the service is
  // active, or empty dictionary otherwise.
  base::Value::Dict GetSettingsWithDefault();

  // Sends the settings to all subscribers. This method should be called by the
  // subclass whenever the settings change.
  void InformSubscribers();

  // Used for persisting the settings. Unlike other PrefStores, this one is not
  // directly hooked up to the PrefService.
  scoped_refptr<PersistentPrefStore> store_;

  bool active_;

  bool initialization_failed_;

  // Set when WaitUntilReadyToSync() is invoked before initialization completes.
  base::OnceClosure wait_until_ready_to_sync_cb_;

  // A set of local settings that are fixed and not configured remotely.
  base::Value::Dict local_settings_;

  SettingsCallbackList settings_callback_list_;

  WebsiteApprovalCallbackList website_approval_callback_list_;

  ShutdownCallbackList shutdown_callback_list_;

  std::unique_ptr<syncer::SyncChangeProcessor> sync_processor_;

  base::WeakPtrFactory<SupervisedUserSettingsService> weak_ptr_factory_{this};
};

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_SETTINGS_SERVICE_H_
