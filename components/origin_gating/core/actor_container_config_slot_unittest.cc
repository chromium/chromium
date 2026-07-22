// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_gating/core/actor_container_config_slot.h"

#include <vector>

#include "base/strings/strcat.h"
#include "base/types/optional_ref.h"
#include "components/origin_gating/core/actor_container_config.h"
#include "net/base/schemeful_site.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

constexpr std::string_view kExampleHost = "example.com";
constexpr std::string_view kOtherHost = "other.com";

namespace origin_gating {

ActorContainerConfig CreateConfigAllowingNavigation(std::string_view domain) {
  net::SchemefulSite site(GURL(base::StrCat({"https://", domain})));
  return ActorContainerConfig({
      {ActorContainerConfig::Location(site),
       ActorContainerConfig::Rule(
           /*navigation_sources=*/{},
           {ActorContainerConfig::Rule::Resource::kSession},
           {ActorContainerConfig::Rule::Capability::kAll})},
  });
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

TEST_F(ActorContainerConfigSlotTest, Assign_EmptyConfig) {
  ActorContainerConfigSlot slot;
  slot.Assign(ActorContainerConfig());
  EXPECT_TRUE(slot.has_value());
  EXPECT_FALSE(slot.value().IsActuationAllowed(kExampleOrigin));
}

TEST_F(ActorContainerConfigSlotTest, Assign_NonemptyConfig) {
  ActorContainerConfigSlot slot;
  slot.Assign(CreateConfigAllowingNavigation(kExampleHost));
  EXPECT_TRUE(slot.has_value());
  EXPECT_TRUE(slot.value().IsActuationAllowed(kExampleOrigin));
}

TEST_F(ActorContainerConfigSlotTest,
       Assign_PresentConfigThenIgnoresSecondCall) {
  ActorContainerConfigSlot slot;
  slot.Assign(CreateConfigAllowingNavigation(kExampleHost));

  slot.Assign(CreateConfigAllowingNavigation(kOtherHost));
  ASSERT_TRUE(slot.has_value());
  EXPECT_FALSE(slot.value().IsActuationAllowed(kOtherOrigin));
  EXPECT_TRUE(slot.value().IsActuationAllowed(kExampleOrigin));
}

}  // namespace origin_gating
