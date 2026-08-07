// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FILLING_AUTOFILL_AI_FIELD_FILLING_ENTITY_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FILLING_AUTOFILL_AI_FIELD_FILLING_ENTITY_UTIL_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/filling/field_filling_util.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-forward.h"
#include "components/autofill/core/common/unique_ids.h"
#include "url/origin.h"

namespace autofill {

class AddressNormalizer;
class AutofillClient;
class AutofillField;
struct AutofillFieldWithAttributeType;
class EntityInstance;
class FormStructure;
class Section;

// Returns the set of entity types that are currently being prefetched for the
// given `field`. We collect all eligible types rather than returning the first
// match to ensure the UI footer correctly decides between showing a
// category-specific manage button (if exactly one section is being loaded) and
// a generic manage button (if multiple sections are being loaded).
DenseSet<EntityType> GetEntityTypesBeingFetched(const AutofillField& field,
                                                const AutofillClient& client);

// Returns the entities from EntityDataManager::GetEntityInstances() for which
// filling is enabled.
std::vector<const EntityInstance*> GetFillableEntityInstances(
    const AutofillClient& client);

// Returns all fields in a `FormStructure` that are fillable by Autofill AI,
// taking into account whether AutofillAI filling is enabled as well as the
// field type predictions and the available entities in `EntityDataManager`.
base::flat_set<FieldGlobalId> GetFieldsFillableByAutofillAi(
    const FormStructure& form,
    const AutofillClient& client);

// Returns the value from `entity` to fill into `field`.
FillingValueAndType GetFillingValueAndTypeForEntity(
    const EntityInstance& entity,
    base::span<const AutofillFieldWithAttributeType> fields_and_types,
    const AutofillField& field,
    mojom::ActionPersistence action_persistence,
    const std::string& app_locale,
    AddressNormalizer* address_normalizer);

// Returns whether filling `form`'s `section` with `entity` would fill sensitive
// attributes.
bool WillFillSensitiveAttributes(const EntityInstance& entity,
                                 const FormStructure& form,
                                 const Section& section,
                                 std::string_view app_locale);

// Returns whether filling `form`'s `section` with `entity` will require a
// server fetch. This returns true only if the form contains fields that match
// sensitive attributes that are currently masked, and the feature
// `features::kAutofillAiWalletPrivatePasses` is enabled.
bool WillRequireServerFetch(const EntityInstance& entity,
                            const FormStructure& form,
                            const Section& section,
                            std::string_view app_locale);

// Returns the origin of the field being targeted, falling back to the
// primary main frame origin if `origin` is opaque.
url::Origin GetTargetFieldOrigin(const url::Origin& origin,
                                 const AutofillClient& client);

// Returns the authentication message shown when reauthenticating with
// biometrics for `origin`.
std::u16string GetAuthenticationMessage(const url::Origin& origin);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FILLING_AUTOFILL_AI_FIELD_FILLING_ENTITY_UTIL_H_
