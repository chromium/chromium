// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AUTOFILL_AI_MANAGEMENT_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AUTOFILL_AI_MANAGEMENT_UTILS_H_

#include <string>

#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"

namespace autofill {

// Returns the i18n string representation of "Add <entity type>". For example,
// for a passport for "en-US", this function should return "Add passport".
std::string GetAddEntityTypeStringForI18n(EntityType entity_type);

// Returns the i18n string representation of "Edit <entity type>". For example,
// for a passport for "en-US", this function should return "Edit passport".
std::string GetEditEntityTypeStringForI18n(EntityType entity_type);

// Returns the i18n string representation of "Delete <entity type>". For
// example, for a passport for "en-US", this function should return "Delete
// passport".
std::string GetDeleteEntityTypeStringForI18n(EntityType entity_type);

// Returns all entities that users can add from the settings page.
DenseSet<EntityType> GetWritableEntityTypes(const GeoIpCountryCode& country_code);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AUTOFILL_AI_MANAGEMENT_UTILS_H_
