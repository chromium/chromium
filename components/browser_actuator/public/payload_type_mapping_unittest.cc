// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/public/payload_type_mapping.h"

#include <set>
#include <string_view>

#include "testing/gtest/include/gtest/gtest.h"

namespace browser_actuator {
namespace {

TEST(PayloadTypeMappingTest, MapsPayloadTypeToWireEnum) {
  EXPECT_EQ(ToDownstreamProtoPayloadType(PayloadType::kControl),
            ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND);
  EXPECT_EQ(ToDownstreamProtoPayloadType(PayloadType::kExperimentalTriggering),
            ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_EXPERIMENTAL_TRIGGERING);
}

TEST(PayloadTypeMappingTest, RoundTripsEveryRoutablePayloadType) {
  for (PayloadType payload_type : kRoutablePayloadTypes) {
    EXPECT_EQ(FromDownstreamProtoPayloadType(
                  ToDownstreamProtoPayloadType(payload_type)),
              payload_type);
  }
}

TEST(PayloadTypeMappingTest, MapsWireEnumToPayloadType) {
  EXPECT_EQ(FromDownstreamProtoPayloadType(
                ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND),
            PayloadType::kControl);
  EXPECT_EQ(FromDownstreamProtoPayloadType(
                ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_EXPERIMENTAL_TRIGGERING),
            PayloadType::kExperimentalTriggering);
}

TEST(PayloadTypeMappingTest, RejectsUnspecifiedWireEnum) {
  EXPECT_EQ(FromDownstreamProtoPayloadType(
                ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_UNSPECIFIED),
            std::nullopt);
}

// A payload type added by a newer server must be reported as unroutable rather
// than mapped onto an arbitrary existing type.
TEST(PayloadTypeMappingTest, RejectsUnknownWireEnum) {
  EXPECT_EQ(FromDownstreamProtoPayloadType(
                static_cast<ActuatorDownstreamPayloadType>(999)),
            std::nullopt);
}

TEST(PayloadTypeMappingTest, ExpectedTypeUrlIsWellFormedForRoutableTypes) {
  for (PayloadType payload_type : kRoutablePayloadTypes) {
    std::string_view type_url = ExpectedTypeUrl(payload_type);
    EXPECT_TRUE(type_url.starts_with(kTypeUrlPrefix)) << type_url;
    // A prefix with nothing after it would match no message at all.
    EXPECT_GT(type_url.size(), kTypeUrlPrefix.size()) << type_url;
  }
}

TEST(PayloadTypeMappingTest, ExpectedTypeUrlIsUniquePerPayloadType) {
  std::set<std::string_view> seen;
  for (PayloadType payload_type : kRoutablePayloadTypes) {
    EXPECT_TRUE(seen.insert(ExpectedTypeUrl(payload_type)).second)
        << "Duplicate type_url: " << ExpectedTypeUrl(payload_type);
  }
}

}  // namespace
}  // namespace browser_actuator
