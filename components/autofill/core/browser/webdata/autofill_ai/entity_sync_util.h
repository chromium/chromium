// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_AI_ENTITY_SYNC_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_AI_ENTITY_SYNC_UTIL_H_

#include <optional>

namespace sync_pb {
class AutofillValuableSpecifics;
}
namespace autofill {

class EntityInstance;

// For a given `EntityInstance`, returns the corresponding
// `sync_pb::AutofillValuableSpecifics`. It is assumed that the entity passed to
// this function is syncable.
sync_pb::AutofillValuableSpecifics CreateSpecificsFromEntityInstance(
    const EntityInstance& entity);

// Converts the given valuable `specifics` into an equivalent EntityInstance.
std::optional<EntityInstance> CreateEntityInstanceFromSpecifics(
    const sync_pb::AutofillValuableSpecifics& specifics);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_AI_ENTITY_SYNC_UTIL_H_
