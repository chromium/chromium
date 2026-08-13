// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AUTOFILL_AI_MANAGEMENT_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AUTOFILL_AI_MANAGEMENT_UTILS_H_

#include <string>
#include <vector>

#include "base/containers/span.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/common/dense_set.h"

namespace autofill {

class EntityInstance;

// Resolves the string ID among the branded arms for the Wallet Pass 2026
// experiment based on `kAutofillAiWalletPassBranding2026StringVariant`.
//
// The experiment has 4 arms total across branding options, and this function
// resolves the string ID for the branded arms 2 through 4:
// - Arm 1 (Flag disabled / unbranded): uses the unbranded entity string ID.
// - Arm 2 (Flag enabled, no variant): uses `default_branded_id`.
// - Arm 3 (Flag enabled, variant 1): uses `variant_1_id`.
// - Arm 4 (Flag enabled, variant 2): uses `variant_2_id`.
int ResolveStringIdsForWalletPass2026Experiment(int default_branded_id,
                                                int variant_1_id,
                                                int variant_2_id);

// Returns the i18n string representation of the "<entity type>s". For example,
// for passport for "en-US", this function should return "Passports".
std::string GetEntityTypeSectionTitleStringForI18n(EntityType entity_type);

// Returns the i18n string representation of "Add <entity type>". For example,
// for a passport for "en-US", this function should return "Add passport".
// If `is_wallet_branded` is true (and supported on the platform), returns the
// branded save/add dialog title, e.g. "Save passport in Google Wallet".
std::string GetAddEntityTypeStringForI18n(EntityType entity_type,
                                          bool is_wallet_branded = false);

// Returns the i18n string representation of "Edit <entity type>". For example,
// for a passport for "en-US", this function should return "Edit passport".
std::string GetEditEntityTypeStringForI18n(EntityType entity_type);

// Returns the i18n string representation of "Delete <entity type>". For
// example, for a passport for "en-US", this function should return "Delete
// passport".
std::string GetDeleteEntityTypeStringForI18n(EntityType entity_type);

// Returns all entities that users can add from the settings page.
DenseSet<EntityType> GetWritableEntityTypes(const GeoIpCountryCode& country_code);

// Returns the entity instances that should be visible in settings.
std::vector<EntityInstance> GetEntityInstancesForSettings(
    base::span<const EntityInstance> entities);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AUTOFILL_AI_MANAGEMENT_UTILS_H_
