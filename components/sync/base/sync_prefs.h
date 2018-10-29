// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_SYNC_PREFS_H_
#define COMPONENTS_SYNC_BASE_SYNC_PREFS_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/prefs/pref_member.h"
#include "components/sync/base/model_type.h"
#include "components/sync/protocol/sync.pb.h"

class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace syncer {

class SyncPrefObserver {
 public:
  // Called whenever the pref that controls whether sync is managed changes.
  virtual void OnSyncManagedPrefChange(bool is_sync_managed) = 0;

 protected:
  virtual ~SyncPrefObserver();
};

// Use this for crypto/passphrase-related parts of sync prefs.
class CryptoSyncPrefs {
 public:
  virtual ~CryptoSyncPrefs();

  // Use this encryption bootstrap token if we're using an explicit passphrase.
  virtual std::string GetEncryptionBootstrapToken() const = 0;
  virtual void SetEncryptionBootstrapToken(const std::string& token) = 0;

  // Use this keystore bootstrap token if we're not using an explicit
  // passphrase.
  virtual std::string GetKeystoreEncryptionBootstrapToken() const = 0;
  virtual void SetKeystoreEncryptionBootstrapToken(
      const std::string& token) = 0;

  // Get/set for flag indicating that passphrase encryption transition is in
  // progress.
  virtual void SetPassphraseEncryptionTransitionInProgress(bool value) = 0;
  virtual bool GetPassphraseEncryptionTransitionInProgress() const = 0;

  // Get/set for saved Nigori specifics that must be passed to backend
  // initialization after transition.
  virtual void SetNigoriSpecificsForPassphraseTransition(
      const sync_pb::NigoriSpecifics& nigori_specifics) = 0;
  virtual void GetNigoriSpecificsForPassphraseTransition(
      sync_pb::NigoriSpecifics* nigori_specifics) const = 0;
};

// SyncPrefs is a helper class that manages getting, setting, and persisting
// global sync preferences. It is not thread-safe, and lives on the UI thread.
class SyncPrefs : public CryptoSyncPrefs,
                  public base::SupportsWeakPtr<SyncPrefs> {
 public:
  // |pref_service| must not be null and must outlive this object.
  explicit SyncPrefs(PrefService* pref_service);
  ~SyncPrefs() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  void AddSyncPrefObserver(SyncPrefObserver* sync_pref_observer);
  void RemoveSyncPrefObserver(SyncPrefObserver* sync_pref_observer);

  // Clears "bookkeeping" sync preferences, such as the last synced time,
  // whether the last shutdown was clean, etc. Does *not* clear sync preferences
  // which are directly user-controlled, such as the set of preferred data
  // types.
  void ClearPreferences();

  // Getters and setters for global sync prefs.

  bool IsFirstSetupComplete() const;
  void SetFirstSetupComplete();

  bool SyncHasAuthError() const;
  void SetSyncAuthError(bool error);

  bool IsSyncRequested() const;
  void SetSyncRequested(bool is_requested);

  base::Time GetLastSyncedTime() const;
  void SetLastSyncedTime(base::Time time);

  base::Time GetLastPollTime() const;
  void SetLastPollTime(base::Time time);

  base::TimeDelta GetShortPollInterval() const;
  void SetShortPollInterval(base::TimeDelta interval);

  base::TimeDelta GetLongPollInterval() const;
  void SetLongPollInterval(base::TimeDelta interval);

  bool HasKeepEverythingSynced() const;
  void SetKeepEverythingSynced(bool keep_everything_synced);

  // The returned set is guaranteed to be a subset of
  // |registered_types|.  Returns |registered_types| directly if
  // HasKeepEverythingSynced() is true.
  // |user_events_separate_pref_group| is true when USER_EVENTS model type has
  // a separate pref group instead of being bundled with the TYPED_URLS. This
  // is used when Unified Consent is enabled.
  //
  // TODO(https://crbug.com/862983): |user_events_separate_pref_group| is only
  // temporary and should removed once Unified Consent feature is is launched.
  ModelTypeSet GetPreferredDataTypes(
      ModelTypeSet registered_types,
      bool user_events_separate_pref_group) const;

  // |preferred_types| should be a subset of |registered_types|.  All
  // types in |preferred_types| are marked preferred, and all types in
  // |registered_types| \ |preferred_types| are marked not preferred.
  // Changes are still made to the prefs even if
  // HasKeepEverythingSynced() is true, but won't be visible until
  // SetKeepEverythingSynced(false) is called.
  // |user_events_separate_pref_group| is true when USER_EVENTS model type has
  // a separate pref group instead of being bundled with the TYPED_URLS. This
  // is used when Unified Consent is enabled.
  //
  // TODO(https://crbug.com/862983): |user_events_separate_pref_group| is only
  // temporary and should removed once Unified Consent feature is is launched.
  void SetPreferredDataTypes(ModelTypeSet registered_types,
                             ModelTypeSet preferred_types,
                             bool user_events_separate_pref_group);

  // Whether Sync is forced off by enterprise policy. Note that this only covers
  // one out of two types of policy, "browser" policy. The second kind, "cloud"
  // policy, is handled directly in ProfileSyncService.
  bool IsManaged() const;

  // Use this encryption bootstrap token if we're using an explicit passphrase.
  std::string GetEncryptionBootstrapToken() const override;
  void SetEncryptionBootstrapToken(const std::string& token) override;

  // Use this keystore bootstrap token if we're not using an explicit
  // passphrase.
  std::string GetKeystoreEncryptionBootstrapToken() const override;
  void SetKeystoreEncryptionBootstrapToken(const std::string& token) override;

  // Maps |type| to its corresponding preference name.
  static const char* GetPrefNameForDataType(ModelType type);

#if defined(OS_CHROMEOS)
  // Use this spare bootstrap token only when setting up sync for the first
  // time.
  std::string GetSpareBootstrapToken() const;
  void SetSpareBootstrapToken(const std::string& token);
#endif

  // Get/set/clear first sync time of current user. Used to roll back browsing
  // data later when user signs out.
  base::Time GetFirstSyncTime() const;
  void SetFirstSyncTime(base::Time time);
  void ClearFirstSyncTime();

  // Out of band sync passphrase prompt getter/setter.
  bool IsPassphrasePrompted() const;
  void SetPassphrasePrompted(bool value);

  // For testing.
  void SetManagedForTest(bool is_managed);

  // Get/Set number of memory warnings received.
  int GetMemoryPressureWarningCount() const;
  void SetMemoryPressureWarningCount(int value);

  // Check if the previous shutdown was clean.
  bool DidSyncShutdownCleanly() const;

  // Set whether the last shutdown was clean.
  void SetCleanShutdown(bool value);

  // Get/set for the last known sync invalidation versions.
  void GetInvalidationVersions(
      std::map<ModelType, int64_t>* invalidation_versions) const;
  void UpdateInvalidationVersions(
      const std::map<ModelType, int64_t>& invalidation_versions);

  // Will return the contents of the LastRunVersion preference. This may be an
  // empty string if no version info was present, and is only valid at
  // Sync startup time (after which the LastRunVersion preference will have been
  // updated to the current version).
  std::string GetLastRunVersion() const;
  void SetLastRunVersion(const std::string& current_version);

  // Get/set for flag indicating that passphrase encryption transition is in
  // progress.
  void SetPassphraseEncryptionTransitionInProgress(bool value) override;
  bool GetPassphraseEncryptionTransitionInProgress() const override;

  // Get/set for saved Nigori specifics that must be passed to backend
  // initialization after transition.
  void SetNigoriSpecificsForPassphraseTransition(
      const sync_pb::NigoriSpecifics& nigori_specifics) override;
  void GetNigoriSpecificsForPassphraseTransition(
      sync_pb::NigoriSpecifics* nigori_specifics) const override;

  // Gets the local sync backend enabled state.
  bool IsLocalSyncEnabled() const;

  // Returns a ModelTypeSet based on |types| expanded to include pref groups
  // (see |pref_groups_|), but as a subset of |registered_types|.
  // Exposed for testing.
  static ModelTypeSet ResolvePrefGroups(ModelTypeSet registered_types,
                                        ModelTypeSet types,
                                        bool user_events_separate_pref_group);

 private:
  static void RegisterDataTypePreferredPref(
      user_prefs::PrefRegistrySyncable* prefs,
      ModelType type,
      bool is_preferred);
  bool GetDataTypePreferred(ModelType type) const;
  void SetDataTypePreferred(ModelType type, bool is_preferred);

  void OnSyncManagedPrefChanged();

  // Never null.
  PrefService* const pref_service_;

  base::ObserverList<SyncPrefObserver>::Unchecked sync_pref_observers_;

  // The preference that controls whether sync is under control by
  // configuration management.
  BooleanPrefMember pref_sync_managed_;

  bool local_sync_enabled_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(SyncPrefs);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_SYNC_PREFS_H_
