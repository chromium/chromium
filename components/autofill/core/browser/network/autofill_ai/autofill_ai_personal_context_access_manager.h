// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_AUTOFILL_AI_PERSONAL_CONTEXT_ACCESS_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_AUTOFILL_AI_PERSONAL_CONTEXT_ACCESS_MANAGER_H_

#include <optional>
#include <string>

#include "base/observer_list_types.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/keyed_service/core/keyed_service.h"

namespace autofill {

class EntityType;

// Manages access to personal context data for autofill ai, including caching
// and ttl management.
// Instantiated once per profile/context.
class AutofillAiPersonalContextAccessManager : public KeyedService {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when asynchronous prefetching of entities through
    // `PrefetchContext()` finishes. If prefetching succeeded, `entities`
    // contains the result. In case it failed or the response was empty,
    // `entities` is empty.
    virtual void OnPrefetchContextComplete(
        const AutofillAiPersonalContextAccessManager& manager,
        std::optional<base::span<const EntityInstance>> entities) {}
    // Called whenever a prefetched entity reaches its TTL or expires for
    // another reason (eligibility to pContext changed, etc).
    virtual void OnMaskedEntityTypeEvicted(
        const AutofillAiPersonalContextAccessManager& manager,
        EntityType type) {}
  };

  enum class RequestStatus {
    kNotStarted,
    kPending,
    kSuccess,
    kFailure,
  };

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Callback for `GetUnmaskedSpiiEntity` requests. On success, it returns
  // the unmasked `EntityInstance` corresponding to the requested `id`.
  // Returns `std::nullopt` on failure (e.g., if the entity is not found or
  // the network request fails).
  using GetUnmaskedSpiiEntityCallback =
      base::OnceCallback<void(std::optional<EntityInstance>)>;

  ~AutofillAiPersonalContextAccessManager() override = default;

  // Fetches personal context from the personal context service.
  virtual void PrefetchContext(
      base::span<const EntityType> requested_types) = 0;

  virtual RequestStatus GetPrefetchStatusByEntityType(
      EntityType type) const = 0;

  // Retrieves the unmasked SPII `EntityInstance` with the given `id`. If it is
  // in the cache, runs the `callback` immediately with the cached value.
  // Otherwise, triggers a network request and runs the `callback` with the
  // result. If the request fails synchronously, runs the `callback` with
  // `std::nullopt`.
  virtual void GetUnmaskedSpiiEntity(
      const EntityInstance::EntityId& id,
      GetUnmaskedSpiiEntityCallback callback) = 0;

  // Returns true if all entities of the given `type_name` have been prefetched
  // and the validity of the result has not expired.
  virtual bool IsTypePrefetched(EntityType type) const = 0;

  // Returns true if a presence signal for `type` is currently available.
  // TODO(crbug.com/516721244): Rename to ServerHasSpiiPresenceSignal.
  virtual bool ServerHasDataAvailable(EntityType type) const = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_AUTOFILL_AI_PERSONAL_CONTEXT_ACCESS_MANAGER_H_
