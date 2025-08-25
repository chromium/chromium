// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_TILES_ENTERPRISE_ENTERPRISE_SHORTCUTS_STORE_H_
#define COMPONENTS_NTP_TILES_ENTERPRISE_ENTERPRISE_SHORTCUTS_STORE_H_

#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "components/ntp_tiles/enterprise/enterprise_shortcut.h"
#include "components/ntp_tiles/pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"

class PrefService;

namespace ntp_tiles {

// A helper class for reading enterprise shortcuts from the profile's
// preference file. All virtual functions are for testing.
class EnterpriseShortcutsStore {
 public:
  static const char kDictionaryKeyUrl[];
  static const char kDictionaryKeyTitle[];
  static const char kDictionaryKeyPolicyOrigin[];
  static const char kDictionaryKeyIsHiddenByUser[];
  static const char kDictionaryKeyAllowUserEdit[];
  static const char kDictionaryKeyAllowUserDelete[];

  explicit EnterpriseShortcutsStore(PrefService* prefs);

  EnterpriseShortcutsStore(const EnterpriseShortcutsStore&) = delete;
  EnterpriseShortcutsStore& operator=(const EnterpriseShortcutsStore&) = delete;

  // Virtual for testing.
  virtual ~EnterpriseShortcutsStore();

  // Retrieves the enterprise custom link data from the profile's preferences
  // and returns them as a list of |EnterpriseShortcut|s. Links from policy will
  // be retrieved unless user modifications have been stored in
  // `prefs::kEnterpriseShortcutsUserList`. If there is a problem with
  // retrieval, an empty list is returned.
  std::vector<EnterpriseShortcut> RetrieveLinks();

  // Stores the provided |links| to the profile's preferences.
  // Virtual for testing.
  virtual void StoreLinks(const std::vector<EnterpriseShortcut>& links);

  // Clears any enterprise custom link data from the profile's preferences.
  // Virtual for testing.
  virtual void ClearLinks();

  // Registers a callback that will be invoked when the links change.
  base::CallbackListSubscription RegisterCallbackForOnChanged(
      base::RepeatingClosure callback);

  // Register EnterpriseShortcutsStore related prefs in the Profile prefs.
  static void RegisterProfilePrefs(PrefRegistrySimple* user_prefs);

 private:
  std::vector<EnterpriseShortcut> RetrieveLinksFromPrefs(
      std::string_view pref_path);
  std::vector<EnterpriseShortcut> RetrieveUserLinks();
  std::vector<EnterpriseShortcut> RetrievePolicyLinks();
  void MergeLinkFromPolicy(std::vector<EnterpriseShortcut>& list_links,
                           const EnterpriseShortcut& link_from_policy);
  void OnPreferenceChanged();

  PrefChangeRegistrar pref_change_registrar_;
  // The pref service used to persist the enterprise custom link data.
  raw_ptr<PrefService> prefs_;
  base::RepeatingClosureList closure_list_;
  base::WeakPtrFactory<EnterpriseShortcutsStore> weak_ptr_factory_{this};
};

}  // namespace ntp_tiles

#endif  // COMPONENTS_NTP_TILES_ENTERPRISE_ENTERPRISE_SHORTCUTS_STORE_H_
