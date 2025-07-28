// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_PREF_STORE_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_PREF_STORE_H_

#include <memory>
#include <string_view>

#include "base/callback_list.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "components/prefs/pref_store.h"
#include "components/prefs/pref_value_map.h"
#include "components/supervised_user/core/browser/supervised_user_content_filters_service.h"
#include "components/supervised_user/core/common/supervised_users.h"
#include "base/memory/weak_ptr.h"

class PrefValueMap;

namespace supervised_user {
class SupervisedUserSettingsService;

// Writes default values to `pref_values` as used within the
// SupervisedUserPrefStore. In this context "default" doesn't indicate the
// bottom pref store in hierarchy which yields fallback values for the
// PrefService, but rather preset values within a single PrefStore.
void SetSupervisedUserPrefStoreDefaults(PrefValueMap& pref_values);

}  // namespace supervised_user

// A PrefStore that gets its values from supervised user settings via the
// SupervisedUserSettingsService passed in at construction.
class SupervisedUserPrefStore : public PrefStore {
 public:
  // Construct a pref store that needs to be manually initialized with Init().
  // Used on iOS since the iOS SupervisedUserSettingsService depends on the
  // creation of the pref service and of this pref store.
  SupervisedUserPrefStore();

  // Construct the pref store on platforms with the settings service available.
  explicit SupervisedUserPrefStore(
      supervised_user::SupervisedUserSettingsService*
          supervised_user_settings_service,
      supervised_user::SupervisedUserContentFiltersService*
          supervised_user_content_filters_service);

  // Subscribe to the settings service.
  void Init(supervised_user::SupervisedUserSettingsService*
                supervised_user_settings_service,
            supervised_user::SupervisedUserContentFiltersService*
                supervised_user_content_filters_service);

  // PrefStore overrides:
  bool GetValue(std::string_view key, const base::Value** value) const override;
  base::Value::Dict GetValues() const override;
  void AddObserver(PrefStore::Observer* observer) override;
  void RemoveObserver(PrefStore::Observer* observer) override;
  bool HasObservers() const override;
  bool IsInitializationComplete() const override;

  void OnNewSettingsAvailable(const base::Value::Dict& settings);

  void OnNewContentFiltersStateAvailable(supervised_user::SupervisedUserContentFiltersService::State state);

  // Notifies observers about changes in the prefs_ compared to the diff_base.
  void NotifyObserversAboutChanges(std::unique_ptr<PrefValueMap> diff_base);

 private:
  ~SupervisedUserPrefStore() override;

  void OnSettingsServiceShutdown();

  base::CallbackListSubscription user_settings_subscription_;

  base::CallbackListSubscription content_filter_settings_subscription_;

  base::CallbackListSubscription shutdown_subscription_;

  std::unique_ptr<PrefValueMap> prefs_;

  base::ObserverList<PrefStore::Observer, true> observers_;

  base::WeakPtrFactory<SupervisedUserPrefStore> weak_factory_{this};
};

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_PREF_STORE_H_
