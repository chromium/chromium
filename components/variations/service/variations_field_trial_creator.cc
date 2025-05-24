// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/service/variations_field_trial_creator.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <cstdint>
#include <memory>
#include <set>
#include <utility>

#include "base/functional/bind.h"
#include "components/language/core/browser/locale_util.h"
#include "components/prefs/pref_service.h"
#include "components/variations/service/safe_seed_manager.h"
#include "components/variations/service/variations_service_client.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace variations {

VariationsFieldTrialCreator::VariationsFieldTrialCreator(
    VariationsServiceClient* client,
    std::unique_ptr<VariationsSeedStore> seed_store,
    const UIStringOverrider& ui_string_overrider,
    LimitedEntropySyntheticTrial* limited_entropy_synthetic_trial)
    : VariationsFieldTrialCreatorBase(
          client,
          std::move(seed_store),
          base::BindOnce([](PrefService* local_state) {
            return language::GetApplicationLocale(local_state);
          }),
          limited_entropy_synthetic_trial),
      ui_string_overrider_(ui_string_overrider) {}

VariationsFieldTrialCreator::~VariationsFieldTrialCreator() = default;

void VariationsFieldTrialCreator::OverrideCachedUIStrings() {
  DCHECK(ui::ResourceBundle::HasSharedInstance());

  ui::ResourceBundle* bundle = &ui::ResourceBundle::GetSharedInstance();
  bundle->CheckCanOverrideStringResources();

  for (auto const& it : overridden_strings_map_)
    bundle->OverrideLocaleStringResource(it.first, it.second);

  overridden_strings_map_.clear();
}

bool VariationsFieldTrialCreator::IsOverrideResourceMapEmpty() {
  return overridden_strings_map_.empty();
}

void VariationsFieldTrialCreator::OverrideUIString(uint32_t resource_hash,
                                                   const std::u16string& str) {
  int resource_id = ui_string_overrider_.GetResourceIndex(resource_hash);
  if (resource_id == -1)
    return;

  // This function may be called before the resource bundle is initialized. So
  // we cache the UI strings and override them after the full browser starts.
  if (!ui::ResourceBundle::HasSharedInstance()) {
    overridden_strings_map_[resource_id] = str;
    return;
  }

  ui::ResourceBundle::GetSharedInstance().OverrideLocaleStringResource(
      resource_id, str);
}

}  // namespace variations
