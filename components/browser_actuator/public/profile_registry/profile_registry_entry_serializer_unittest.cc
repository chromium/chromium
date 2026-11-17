// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/public/profile_registry/profile_registry_entry_serializer.h"

#include <cstdint>
#include <optional>
#include <string>

#include "base/containers/span.h"
#include "components/browser_actuator/public/profile_registry/proto/chrome_profile_registry_entry.pb.h"
#include "google/protobuf/any.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_actuator {
namespace {

ChromeProfileRegistryEntry MakeEntry() {
  ChromeProfileRegistryEntry entry;
  entry.set_gaia_id("obfuscated-gaia-id");
  entry.set_profile_dir("Profile 1");
  entry.set_bundle_id("com.google.Chrome.canary");
  entry.set_mach_service_name(
      "com.google.Chrome.canary.local-actuation."
      "3f1a9c6e5d2b4a7f8e9d0c1b2a3f4e5d6c7b8a9f0e1d2c3b4a5f6e7d8c9b0a1f");
  return entry;
}

base::span<const uint8_t> AsBytes(const std::string& s) {
  return base::as_byte_span(s);
}

TEST(ProfileRegistryEntrySerializerTest, RoundTripPreservesEveryField) {
  const ChromeProfileRegistryEntry entry = MakeEntry();
  const std::string serialized = SerializeProfileRegistryEntryAsAny(entry);

  const std::optional<ChromeProfileRegistryEntry> parsed =
      ParseProfileRegistryEntryFromAny(AsBytes(serialized));
  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ(parsed->gaia_id(), entry.gaia_id());
  EXPECT_EQ(parsed->profile_dir(), entry.profile_dir());
  EXPECT_EQ(parsed->bundle_id(), entry.bundle_id());
  EXPECT_EQ(parsed->mach_service_name(), entry.mach_service_name());
}

TEST(ProfileRegistryEntrySerializerTest, AnyCarriesTheExpectedTypeUrl) {
  const std::string serialized =
      SerializeProfileRegistryEntryAsAny(MakeEntry());

  google::protobuf::Any any_proto;
  ASSERT_TRUE(any_proto.ParseFromString(serialized));
  // Literal string: external readers match this exact type_url.
  EXPECT_EQ(any_proto.type_url(),
            "type.googleapis.com/browser_actuator.ChromeProfileRegistryEntry");
}

TEST(ProfileRegistryEntrySerializerTest, RejectsBytesThatAreNotAnAny) {
  const std::string garbage = "\xff\xff\xff not a protobuf";
  EXPECT_FALSE(ParseProfileRegistryEntryFromAny(AsBytes(garbage)).has_value());
}

TEST(ProfileRegistryEntrySerializerTest, RejectsAnyWithForeignTypeUrl) {
  google::protobuf::Any any_proto;
  any_proto.set_type_url(
      "type.googleapis.com/browser_actuator.ChromeColocationProof");
  any_proto.set_value(MakeEntry().SerializeAsString());

  EXPECT_FALSE(
      ParseProfileRegistryEntryFromAny(AsBytes(any_proto.SerializeAsString()))
          .has_value());
}

TEST(ProfileRegistryEntrySerializerTest, RejectsAnyWhoseValueDoesNotParse) {
  google::protobuf::Any any_proto;
  any_proto.set_type_url(
      "type.googleapis.com/browser_actuator.ChromeProfileRegistryEntry");
  // A length-delimited field whose declared length runs past the end.
  any_proto.set_value(std::string("\x0a\x7f", 2));

  EXPECT_FALSE(
      ParseProfileRegistryEntryFromAny(AsBytes(any_proto.SerializeAsString()))
          .has_value());
}

TEST(ProfileRegistryEntrySerializerTest,
     PreservesUnknownFieldsForForwardCompat) {
  // Simulate an entry written by a newer Chrome that added field 5: append a
  // string field with tag (5 << 3 | 2) to a serialized entry.
  std::string value = MakeEntry().SerializeAsString();
  const std::string extra = "future";
  value.push_back(static_cast<char>((5 << 3) | 2));
  value.push_back(static_cast<char>(extra.size()));
  value += extra;

  google::protobuf::Any any_proto;
  any_proto.set_type_url(
      "type.googleapis.com/browser_actuator.ChromeProfileRegistryEntry");
  any_proto.set_value(value);

  const std::optional<ChromeProfileRegistryEntry> parsed =
      ParseProfileRegistryEntryFromAny(AsBytes(any_proto.SerializeAsString()));
  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ(parsed->profile_dir(), "Profile 1");
  // Re-serializing preserves unknown fields from newer writers.
  EXPECT_EQ(parsed->SerializeAsString(), value);
}

}  // namespace
}  // namespace browser_actuator
