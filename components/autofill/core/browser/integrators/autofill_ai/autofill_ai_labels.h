// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AUTOFILL_AI_AUTOFILL_AI_LABELS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AUTOFILL_AI_AUTOFILL_AI_LABELS_H_

#include <map>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/types/optional_ref.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/common/dense_set.h"

namespace autofill {

class EntityInstance;

// Separator to use between a certain entity label attributes, for example:
// "Passport · Jon Doe · Germany".
inline constexpr char16_t kLabelSeparator[] = u" · ";

// During label computation, every entity's label is a vector of non-empty
// strings (which the UI later concatenates).
using EntityLabel = std::vector<std::u16string>;

// Returns a vector of EntityLabels, with one entry for each EntityInstance in
// `entities`.
//
// That is, the `i`th element of the returned vector corresponds to
// `entities[i]`. The individual EntityLabels may be empty, but the strings they
// contain are non-empty.
//
// This is for example used by filling suggestions and the settings page.
//
// `attribute_types_to_ignore` contains types that shouldn't be used to generate
// labels.
//
// If `only_disambiguating_types` is true, only `AttributeType`s satisfying
// `AttributeType::is_disambiguating_type()` are considered. For example, for a
// passport, the name and country are considered, but the number is not.
std::vector<EntityLabel> GetLabelsForEntities(
    base::span<const EntityInstance* const> entities,
    DenseSet<AttributeType> attribute_types_to_ignore,
    bool only_disambiguating_types,
    const std::string& app_locale);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AUTOFILL_AI_AUTOFILL_AI_LABELS_H_
