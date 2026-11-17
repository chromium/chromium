// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/public/profile_registry/profile_registry_entry_serializer.h"

#include <cstdint>
#include <optional>
#include <string>

#include "base/containers/span.h"
#include "base/strings/strcat.h"
#include "base/strings/string_view_util.h"
#include "components/browser_actuator/public/profile_registry/proto/chrome_profile_registry_entry.pb.h"
#include "google/protobuf/any.pb.h"

namespace browser_actuator {

namespace {

constexpr char kTypeUrlPrefix[] = "type.googleapis.com/";

}  // namespace

// LITE_RUNTIME messages lack reflection, so Any::PackFrom and Any::UnpackTo
// are unavailable.
std::string SerializeProfileRegistryEntryAsAny(
    const ChromeProfileRegistryEntry& entry) {
  google::protobuf::Any any_proto;
  any_proto.set_type_url(base::StrCat({kTypeUrlPrefix, entry.GetTypeName()}));
  any_proto.set_value(entry.SerializeAsString());
  return any_proto.SerializeAsString();
}

std::optional<ChromeProfileRegistryEntry> ParseProfileRegistryEntryFromAny(
    base::span<const uint8_t> serialized_any) {
  google::protobuf::Any any_proto;
  if (!any_proto.ParseFromString(base::as_string_view(serialized_any))) {
    return std::nullopt;
  }

  if (any_proto.type_url() !=
      base::StrCat(
          {kTypeUrlPrefix,
           ChromeProfileRegistryEntry::default_instance().GetTypeName()})) {
    return std::nullopt;
  }
  ChromeProfileRegistryEntry entry;
  if (!entry.ParseFromString(any_proto.value())) {
    return std::nullopt;
  }
  return entry;
}

}  // namespace browser_actuator
