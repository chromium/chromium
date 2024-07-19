// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_INVALIDATION_FACTORY_H_
#define COMPONENTS_INVALIDATION_INVALIDATION_FACTORY_H_

#include <memory>
#include <string>
#include <variant>

#include "base/feature_list.h"
#include "base/memory/scoped_refptr.h"
#include "components/invalidation/invalidation_listener.h"
#include "components/invalidation/public/invalidation_service.h"

class PrefService;

namespace gcm {
class GCMDriver;
}

namespace instance_id {
class InstanceIDDriver;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace invalidation {

// Turns on invalidations with direct messages by substituting
// InvalidationService with InvalidationListener.
BASE_DECLARE_FEATURE(kInvalidationsWithDirectMessages);

class IdentityProvider;

std::variant<std::unique_ptr<InvalidationService>,
             std::unique_ptr<InvalidationListener>>
CreateInvalidationServiceOrListener(
    IdentityProvider* identity_provider,
    gcm::GCMDriver* gcm_driver,
    instance_id::InstanceIDDriver* instance_id_driver,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    PrefService* pref_service,
    std::string sender_id,
    std::string project_number,
    std::string log_prefix);

// Converts a variant of unique pointers to a corresponding variant of raw
// pointers.
template <typename T, typename U>
auto UniquePointerVariantToPointer(
    const std::variant<std::unique_ptr<T>, std::unique_ptr<U>>& v) {
  return std::visit(
      [](auto&& arg) -> std::variant<T*, U*> { return arg.get(); }, v);
}

// Converts a variant of raw pointers to a corresponding variant of `raw_ptr`.
template <typename T, typename U>
auto PointerVariantToRawPointer(const std::variant<T*, U*>& v) {
  return std::visit(
      [](auto&& arg) -> std::variant<raw_ptr<T>, raw_ptr<U>> { return arg; },
      v);
}

// Converts a variant of `raw_ptr` to a corresponding variant of raw pointers.
template <typename T, typename U>
auto RawPointerVariantToPointer(const std::variant<raw_ptr<T>, raw_ptr<U>>& v) {
  return std::visit([](auto&& arg) -> std::variant<T*, U*> { return arg; }, v);
}

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_INVALIDATION_FACTORY_H_
