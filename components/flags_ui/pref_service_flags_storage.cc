// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/flags_ui/pref_service_flags_storage.h"

#include "base/values.h"
#include "build/build_config.h"
#include "components/flags_ui/flags_ui_pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace flags_ui {

PrefServiceFlagsStorage::PrefServiceFlagsStorage(PrefService* prefs)
    : prefs_(prefs) {}

PrefServiceFlagsStorage::~PrefServiceFlagsStorage() {}

std::set<std::string> PrefServiceFlagsStorage::GetFlags() {
  const base::ListValue* enabled_experiments =
      prefs_->GetList(prefs::kEnabledLabsExperiments);
  std::set<std::string> flags;
  for (auto it = enabled_experiments->begin(); it != enabled_experiments->end();
       ++it) {
    std::string experiment_name;
    if (!it->GetAsString(&experiment_name)) {
      LOG(WARNING) << "Invalid entry in " << prefs::kEnabledLabsExperiments;
      continue;
    }
    flags.insert(experiment_name);
  }
  return flags;
}

bool PrefServiceFlagsStorage::SetFlags(const std::set<std::string>& flags) {
  ListPrefUpdate update(prefs_, prefs::kEnabledLabsExperiments);
  base::ListValue* experiments_list = update.Get();

  experiments_list->Clear();
  for (auto it = flags.begin(); it != flags.end(); ++it) {
    experiments_list->AppendString(*it);
  }

  return true;
}

void PrefServiceFlagsStorage::CommitPendingWrites() {
  prefs_->CommitPendingWrite();
}

// static
void PrefServiceFlagsStorage::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kEnabledLabsExperiments);
}

#if defined(OS_CHROMEOS)
// static
void PrefServiceFlagsStorage::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(prefs::kEnabledLabsExperiments);
}
#endif  // defined(OS_CHROMEOS)

}  // namespace flags_ui
