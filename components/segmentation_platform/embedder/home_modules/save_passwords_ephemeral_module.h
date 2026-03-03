// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_SAVE_PASSWORDS_EPHEMERAL_MODULE_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_SAVE_PASSWORDS_EPHEMERAL_MODULE_H_

#include <map>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/segmentation_platform/embedder/home_modules/card_selection_info.h"
#include "components/segmentation_platform/embedder/home_modules/constants.h"

namespace segmentation_platform::home_modules {

// `SavePasswordsEphemeralModule` represents an ephemeral Magic Stack module for
// the Save Passwords tip. It is responsible for determining whether the module
// should be shown to the user based on various signals and the user's
// interaction history.
class SavePasswordsEphemeralModule : public CardSelectionInfo {
 public:
  explicit SavePasswordsEphemeralModule(PrefService* profile_prefs)
      : CardSelectionInfo(kSavePasswordsEphemeralModule),
        profile_prefs_(profile_prefs) {}
  ~SavePasswordsEphemeralModule() override = default;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Returns `true` if the given label corresponds to a
  // `SavePasswordsEphemeralModule` variation.
  static bool IsModuleLabel(std::string_view label);

  // Returns `true` if the `SavePasswordsEphemeralModule` should be
  // enabled.
  static bool IsEnabled(PrefService* profile_prefs);

  // `CardSelectionInfo` overrides.
  std::map<SignalKey, FeatureQuery> GetInputs() override;
  ShowResult ComputeCardResult(
      const CardSelectionSignals& signals) const override;
  void OnShow(PrefService* profile_prefs, PrefService* local_state) override;
  void OnInteract(PrefService* profile_prefs,
                  PrefService* local_state) override;

 private:
  raw_ptr<PrefService> profile_prefs_;
};

}  // namespace segmentation_platform::home_modules

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_SAVE_PASSWORDS_EPHEMERAL_MODULE_H_
