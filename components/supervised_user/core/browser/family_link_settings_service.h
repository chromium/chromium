// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_FAMILY_LINK_SETTINGS_SERVICE_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_FAMILY_LINK_SETTINGS_SERVICE_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_store.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/supervised_user/core/common/supervised_users.h"
#include "components/sync/model/syncable_service.h"

class PersistentPrefStore;

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace supervised_user {

// This class syncs Family Link user settings from the sync server, which are
// mapped to preferences. The downloaded settings are either persisted in the
// SupervisedUserPrefStore - second most important store of the PrefService, or
// are available directly using accessors. Note: Family Link settings are one of
// many sources for user supervision; but this class uses generic name
// "FamilyLinkSettingsService" for historical reasons.
//
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
class FamilyLinkSettingsService : public KeyedService,
                                  public syncer::SyncableService,
                                  public PrefStore::Observer {
 public:
  // A callback whose first parameter is a dictionary containing all Family Link
  // user settings. If the dictionary is empty, it means that the service is
  // inactive, i.e. the user is not supervised.
  using SettingsCallback =
      base::RepeatingCallback<void(const base::DictValue&)>;

  // Called when a new host is remotely approved for this Family Link user. The
  // first param is newly approved host, which might be a pattern containing
  // wildcards (e.g. "*.google.*"").
  using WebsiteApprovalCallback =
      base::RepeatingCallback<void(const std::string& hostname)>;

  using ShutdownCallback = base::RepeatingCallback<void()>;

  // Host exceptions from Family Link Settings. It is possible that the same
  // host in on both lists (allowed and blocked).
  struct HostExceptions {
    HostExceptions();
    HostExceptions(const HostExceptions&);
    HostExceptions& operator=(const HostExceptions&);
    ~HostExceptions();
    std::set<std::string, std::less<>> allowed_hosts;
    std::set<std::string, std::less<>> blocked_hosts;
  };
  // Url exceptions from Family Link Settings. True value means that the url is
  // allowed, false value means that the url is blocked.
  using UrlExceptions = std::map<GURL, bool>;

  FamilyLinkSettingsService();

  FamilyLinkSettingsService(const FamilyLinkSettingsService&) = delete;
  FamilyLinkSettingsService& operator=(const FamilyLinkSettingsService&) =
      delete;

  ~FamilyLinkSettingsService() override;

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

  // Adds a callback to be called when Family Link user settings are initially
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

  // Whether the service is active. If false, it means that the profile is not
  // requesting family link supervised user settings.
  bool IsActive() const { return active_; }

  // Whether Family Link user settings are available.
  bool IsReady() const;

  // Clears all Family Link user settings and items.
  void Clear();

  // Constructs a key for a split Family Link user setting from a prefix and a
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
  void SetLocalSetting(std::string_view key, base::DictValue dict);

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
  std::string GetClientTag(
      const syncer::EntityData& entity_data) const override;

  // Returns a weak pointer to the service.
  base::WeakPtr<const FamilyLinkSettingsService> GetWeakPtr() const;

  // PrefStore::Observer implementation:
  void OnInitializationCompleted(bool success) override;

  bool IsCustomPassphraseAllowed() const;

  const base::DictValue& LocalSettingsForTest() const;

  // Returns the type of web filter that is applied to the current profile.
  WebFilterType GetWebFilterType() const;

  // Returns the default filtering behavior for the current profile, using the
  // current settings.
  FilteringBehavior GetDefaultFilteringBehavior() const;

  // Returns manually allowed and blocked hosts or urls.
  HostExceptions GetHostExceptions() const;
  UrlExceptions GetUrlExceptions() const;

 private:
  // Returns parsed logical value for the default filtering behavior setting,
  // considering its default value.
  FilteringBehavior GetDefaultFilteringBehavior(
      const base::DictValue& settings) const;

  // Returns parsed logical value for the safe sites setting, considering its
  // default value.
  bool IsSafeSitesEnabled(const base::DictValue& settings) const;

  // Returns the dictionary where a given Sync item should be stored, depending
  // on whether the Family Link user setting is atomic or split. In case of a
  // split setting, the split setting prefix of |key| is removed, so that |key|
  // can be used to update the returned dictionary.
  base::DictValue* GetDictionaryAndSplitKey(std::string* key) const;
  base::DictValue* GetOrCreateDictionary(std::string_view key) const;
  base::DictValue* GetAtomicSettings() const;
  base::DictValue* GetSplitSettings() const;
  base::DictValue* GetQueuedItems() const;

  // Returns a dictionary with all Family Link user settings if the service is
  // active, or empty dictionary otherwise.
  base::DictValue GetSettingsWithDefault() const;

  // Sends the settings to all subscribers if settings have changed since the
  // last time a notification was sent.
  void InformSubscribers();

  // Used for persisting the settings. Unlike other PrefStores, this one is not
  // directly hooked up to the PrefService.
  scoped_refptr<PersistentPrefStore> store_;

  bool active_;

  bool initialization_failed_;

  std::optional<base::DictValue> last_notified_settings_;

  // Set when WaitUntilReadyToSync() is invoked before initialization completes.
  base::OnceClosure wait_until_ready_to_sync_cb_;

  // A set of local settings that are fixed and not configured remotely.
  base::DictValue local_settings_;

  base::RepeatingCallbackList<SettingsCallback::RunType>
      settings_callback_list_;

  base::RepeatingCallbackList<WebsiteApprovalCallback::RunType>
      website_approval_callback_list_;

  base::RepeatingCallbackList<ShutdownCallback::RunType>
      shutdown_callback_list_;

  std::unique_ptr<syncer::SyncChangeProcessor> sync_processor_;

  // TODO(crbug.com/480137930): remove once issue is resolved.
  bool wait_until_ready_to_sync_trap_ = false;

  base::WeakPtrFactory<FamilyLinkSettingsService> weak_ptr_factory_{this};
};

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_FAMILY_LINK_SETTINGS_SERVICE_H_
