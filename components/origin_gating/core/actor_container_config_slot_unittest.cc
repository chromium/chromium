// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_gating/core/actor_container_config_slot.h"

#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/origin_gating/core/actor_container_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

constexpr std::string_view kExampleHost = "example.com";
constexpr std::string_view kOtherHost = "other.com";

namespace origin_gating {

using optimization_guide::proto::AgentContainerConfig;

AgentContainerConfig CreateConfigAllowingNavigation(std::string_view domain) {
  AgentContainerConfig config;
  optimization_guide::proto::LocationRule* location =
      config.add_location_rules();
  CHECK(location);
  optimization_guide::proto::Site* site =
      location->mutable_location()->mutable_site();
  CHECK(site);
  site->set_protocol(optimization_guide::proto::PROTOCOL_HTTPS);
  site->set_domain(domain);
  location->mutable_metadata()->add_capabilities(
      optimization_guide::proto::RuleMetadata::CAPABILITY_ALL);
  location->mutable_metadata()->add_accessible_resources(
      optimization_guide::proto::RuleMetadata::RESOURCE_SESSION);
  return config;
}

class ActorContainerConfigSlotTest : public testing::Test {
 public:
  const url::Origin kExampleOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const url::Origin kOtherOrigin =
      url::Origin::Create(GURL("https://other.com"));
};

TEST_F(ActorContainerConfigSlotTest, InitialState) {
  ActorContainerConfigSlot slot;
  EXPECT_FALSE(slot.has_value());
}

TEST_F(ActorContainerConfigSlotTest, Assign_EmptyProto) {
  ActorContainerConfigSlot slot;
  EXPECT_TRUE(slot.Assign(AgentContainerConfig()));
  EXPECT_TRUE(slot.has_value());
  EXPECT_FALSE(slot.value().IsActuationAllowed(kExampleOrigin));
}

TEST_F(ActorContainerConfigSlotTest, Assign_NonemptyProto) {
  ActorContainerConfigSlot slot;
  EXPECT_TRUE(slot.Assign(CreateConfigAllowingNavigation(kExampleHost)));
  EXPECT_TRUE(slot.has_value());
  EXPECT_TRUE(slot.value().IsActuationAllowed(kExampleOrigin));
}

TEST_F(ActorContainerConfigSlotTest, Assign_PresentProtoThenIgnoresSecondCall) {
  ActorContainerConfigSlot slot;
  EXPECT_TRUE(slot.Assign(CreateConfigAllowingNavigation(kExampleHost)));

  EXPECT_FALSE(slot.Assign(CreateConfigAllowingNavigation(kOtherHost)));
  ASSERT_TRUE(slot.has_value());
  EXPECT_FALSE(slot.value().IsActuationAllowed(kOtherOrigin));
}

}  // namespace origin_gating
