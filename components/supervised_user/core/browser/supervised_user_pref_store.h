// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_PREF_STORE_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_PREF_STORE_H_

#include <memory>
#include <string_view>

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "components/prefs/pref_store.h"
#include "components/prefs/pref_value_map.h"
#include "components/supervised_user/core/browser/device_parental_controls.h"
#include "components/supervised_user/core/common/supervised_users.h"

class PrefValueMap;

namespace supervised_user {
class FamilyLinkSettingsService;

// Writes default values to `pref_values` as used within the
// SupervisedUserPrefStore. In this context "default" doesn't indicate the
// bottom pref store in hierarchy which yields fallback values for the
// PrefService, but rather preset values within a single PrefStore.
void SetSupervisedUserPrefStoreDefaults(PrefValueMap& pref_values);

}  // namespace supervised_user

// A PrefStore that gets its values from supervised user settings via the
// FamilyLinkSettingsService passed in at construction.
class SupervisedUserPrefStore : public PrefStore {
 public:
  // Construct a pref store that needs to be manually initialized with Init().
  // Used on iOS since the iOS FamilyLinkSettingsService depends on the
  // creation of the pref service and of this pref store.
  SupervisedUserPrefStore();

  // Construct the pref store with the settings service and device parental
  // controls available.
  SupervisedUserPrefStore(
      supervised_user::FamilyLinkSettingsService* family_link_settings_service,
      supervised_user::DeviceParentalControls& device_parental_controls);

  // Subscribe to the settings service.
  void Init(
      supervised_user::FamilyLinkSettingsService* family_link_settings_service,
      supervised_user::DeviceParentalControls& device_parental_controls);

  // PrefStore overrides:
  bool GetValue(std::string_view key, const base::Value** value) const override;
  base::DictValue GetValues() const override;
  void AddObserver(PrefStore::Observer* observer) override;
  void RemoveObserver(PrefStore::Observer* observer) override;
  bool HasObservers() const override;
  bool IsInitializationComplete() const override;

  void OnNewSettingsAvailable(const base::DictValue& settings);

 private:
  // Local representation of last received the device parental controls state.
  struct DeviceParentalControlsState {
    bool is_web_filtering_enabled = false;
    bool is_incognito_mode_disabled = false;
    bool is_safe_search_forced = false;
    bool is_enabled = false;
  };

  ~SupervisedUserPrefStore() override;

  void OnSettingsServiceShutdown();

  void OnDeviceParentalControlsChanged(
      const supervised_user::DeviceParentalControls& device_parental_controls);

  // Merges the supervised user settings and android parental controls state
  // into a single pref value map. Non-empty `family_link_settings_` will
  // ignore android_parental_controls_state (for now).
  void RecreatePreferences();

  // Notifies observers about changes in the prefs_ compared to the diff_base,
  // which must own a valid pointer.
  void NotifyObserversAboutChanges(std::unique_ptr<PrefValueMap> diff_base);

  base::CallbackListSubscription family_link_settings_subscription_;

  base::CallbackListSubscription device_parental_controls_subscription_;

  base::CallbackListSubscription shutdown_subscription_;

  std::unique_ptr<PrefValueMap> prefs_;

  base::WeakPtr<const supervised_user::FamilyLinkSettingsService>
      family_link_settings_service_;

  base::ObserverList<PrefStore::Observer, true> observers_;

  // Last received family link settings.
  std::optional<base::DictValue> family_link_settings_;

  // Last received device parental controls settings. Default value is
  // semantically equivalent to no value.
  DeviceParentalControlsState device_parental_controls_state_;

  base::WeakPtrFactory<SupervisedUserPrefStore> weak_factory_{this};
};

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_PREF_STORE_H_
