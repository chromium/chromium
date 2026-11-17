// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_PUBLIC_PROFILE_REGISTRY_PROFILE_REGISTRY_ENTRY_SERIALIZER_H_
#define COMPONENTS_BROWSER_ACTUATOR_PUBLIC_PROFILE_REGISTRY_PROFILE_REGISTRY_ENTRY_SERIALIZER_H_

#include <cstdint>
#include <optional>
#include <string>

#include "base/containers/span.h"
#include "components/browser_actuator/public/profile_registry/proto/chrome_profile_registry_entry.pb.h"

namespace browser_actuator {

// Packs `entry` inside a `google.protobuf.Any` and returns the serialized
// `Any`.
std::string SerializeProfileRegistryEntryAsAny(
    const ChromeProfileRegistryEntry& entry);

// Unpacks a `ChromeProfileRegistryEntry` from a serialized
// `google.protobuf.Any`, or returns `std::nullopt` if `serialized_any` is
// malformed or has a different `type_url`.
std::optional<ChromeProfileRegistryEntry> ParseProfileRegistryEntryFromAny(
    base::span<const uint8_t> serialized_any);

}  // namespace browser_actuator

#endif  // COMPONENTS_BROWSER_ACTUATOR_PUBLIC_PROFILE_REGISTRY_PROFILE_REGISTRY_ENTRY_SERIALIZER_H_
