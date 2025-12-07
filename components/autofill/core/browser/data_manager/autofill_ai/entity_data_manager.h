// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_AUTOFILL_AI_ENTITY_DATA_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_AUTOFILL_AI_ENTITY_DATA_MANAGER_H_

#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "base/types/optional_ref.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_instance_cleaner.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_observer.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/webdata/common/web_data_service_consumer.h"

namespace history {
class DeletionInfo;
}  // namespace history

namespace strike_database {
class StrikeDatabaseBase;
}  // namespace strike_database

namespace syncer {
class SyncService;
}  // namespace syncer

namespace autofill {

class AutofillAiSaveStrikeDatabaseByHost;

// Loads, adds, updates, and removes EntityInstances. Deletes data from
// AutofillAI strike databases on history deletion.
//
// These operations are asynchronous; this is similar to
// AutocompleteHistoryManager and unlike AddressDataManager.
//
// There is at most one instance per profile. While incognito profiles have
// their own EntityDataManager instance, they use the same underlying database.
// Therefore, it is the responsibility of the callers to ensure that no data
// from an incognito session is persisted unintentionally.
class EntityDataManager : public KeyedService,
                          public AutofillWebDataServiceObserverOnUISequence,
                          history::HistoryServiceObserver {
 public:
  // Autofill AI enabled pref migration status.
  //
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // LINT.IfChange(AutofillAiPrefMigrationStatus)
  enum class AutofillAiPrefMigrationStatus {
    // This means the `kAutofillAiEnabled` pref was set to true due to
    // the account keyed pref being enabled.
    kPrefMigratedEnabled = 0,
    // This means the `kAutofillAiEnabled` pref was set to false due to
    // the account keyed pref being enabled.
    kPrefMigratedDisabled = 1,
    // This means the `kAutofillAiEnabled` pref was previously enabled or
    // disabled.
    kPrefNotMigratedAlreadySet = 2,
    // This means that the original account keyed pref was never set, therefore
    // no migration happened.
    kPrefNotMigratedAccountPrefNeverSet = 3,
    kMaxValue = kPrefNotMigratedAccountPrefNeverSet
  };
  // LINT.ThenChange(/tools/metrics/histograms/metadata/autofill/enums.xml:AutofillAiPrefMigrationStatus)

  class Observer : public base::CheckedObserver {
   public:
    virtual void OnEntityInstancesChanged() {}
  };

  explicit EntityDataManager(
      PrefService* pref_service,
      const signin::IdentityManager* identity_manager,
      syncer::SyncService* sync_service,
      scoped_refptr<AutofillWebDataService> profile_database,
      history::HistoryService* history_service,
      strike_database::StrikeDatabaseBase* strike_database);
  EntityDataManager(const EntityDataManager&) = delete;
  EntityDataManager& operator=(const EntityDataManager&) = delete;
  ~EntityDataManager() override;

  // Adds an entity if it doesn't exist in the database yet; otherwise updates
  // it.
  void AddOrUpdateEntityInstance(EntityInstance entity);

  // Removes an entity if it exists in the database; otherwise it's a no-op.
  void RemoveEntityInstance(EntityInstance::EntityId guid);

  // Removes all entities in the database whose EntityInstance::date_modified()
  // is in the range.
  // Prefer this function over iterating over GetEntityInstances() and calling
  // RemoveEntityInstance() because this function also removes invalid entities.
  void RemoveEntityInstancesModifiedBetween(base::Time delete_begin,
                                            base::Time delete_end);

  // Returns the cached valid entity instances from the database.
  //
  // The cache is populated asynchronously after the construction of this
  // EntityDataManager. Returns an empty vector until the population is
  // finished.
  //
  // See `EntityTable::GetEntityInstances()` for details on what "valid" means.
  base::span<const EntityInstance> GetEntityInstances() const LIFETIME_BOUND {
    return entities_;
  }

  // Equivalent to looking up `guid` in `GetEntityInstances()`.
  base::optional_ref<const EntityInstance> GetEntityInstance(
      const EntityInstance::EntityId& guid) const LIFETIME_BOUND;

  // Returns if there are any pending queries to the web database.
  bool HasPendingQueries() const;

  // AutofillWebDataServiceObserver:
  void OnAutofillChangedBySync(syncer::DataType data_type) override;

  // history::HistoryServiceObserver:
  void OnHistoryDeletions(history::HistoryService*,
                          const history::DeletionInfo& deletion_info) override;

  // Records the date an entity was used and also increments the number of times
  // it was used.
  void RecordEntityUsed(const EntityInstance::EntityId& guid,
                        base::Time use_date);

  // Notifies the observers that the entity instances have changed.
  void NotifyEntityInstancesChanged();

  void AddObserver(Observer* observer) { observers_.AddObserver(observer); }

  void RemoveObserver(Observer* observer) {
    observers_.RemoveObserver(observer);
  }

 private:
  void LoadEntities();

  base::optional_ref<EntityInstance> GetMutableEntityInstance(
      const EntityInstance::EntityId& guid);

  // Boolean that allows the EntityDataManager to differentiate between initial
  // data loads and data updates.
  bool entity_data_loaded_ = false;

  // Non-null except perhaps in TestEntityDataManager, which overrides all
  // functions that access it.
  const scoped_refptr<AutofillWebDataService> webdata_service_;

  // The ongoing LoadEntities() query.
  WebDataServiceBase::Handle pending_query_{};

  // The result of the last successful LoadEntities() query.
  // All entries are identifiable by their EntityInstance::guid().
  base::flat_set<EntityInstance, EntityInstance::CompareByGuid> entities_;

  base::ScopedObservation<AutofillWebDataService,
                          AutofillWebDataServiceObserverOnUISequence>
      webdata_service_observation_{this};

  base::ScopedObservation<history::HistoryService, HistoryServiceObserver>
      history_service_observation_{this};

  std::unique_ptr<AutofillAiSaveStrikeDatabaseByHost> save_strike_db_by_host_;

  base::ObserverList<Observer> observers_;

  EntityInstanceCleaner entity_instance_cleaner_;

  base::WeakPtrFactory<EntityDataManager> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_AUTOFILL_AI_ENTITY_DATA_MANAGER_H_
