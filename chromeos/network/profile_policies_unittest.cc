// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/profile_policies.h"

#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "components/onc/onc_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

// Creates a LIST value containing clones of elements passed in the initializer
// list.
base::Value NetworkConfigsList(
    std::initializer_list<const base::Value*> network_configs) {
  base::Value result(base::Value::Type::LIST);
  for (const auto* network_config : network_configs) {
    result.Append(network_config->Clone());
  }
  return result;
}

// Creates a very basic for-testing NetworkConfig, essentially
// {
//   "guid": <passed_guid>
// }
base::Value NetworkConfig(base::StringPiece guid) {
  base::Value result(base::Value::Type::DICTIONARY);
  result.SetKey(::onc::network_config::kGUID, base::Value(guid));
  return result;
}

bool FalseShillPropertiesMatcher(const base::Value& onc_network_configuration,
                                 const base::Value& shill_properties) {
  return false;
}

bool EqShillPropertiesMatcher(const base::Value& onc_network_configuration,
                              const base::Value& shill_properties) {
  return onc_network_configuration == shill_properties;
}

using ProfilePoliciesTest = ::testing::Test;

// Calls GetGlobalNetworkConfig when no GlobalNetworkConfig has been set yet.
TEST_F(ProfilePoliciesTest, GlobalNetworkConfigIsEmpty) {
  ProfilePolicies profile_policies;
  ASSERT_TRUE(profile_policies.GetGlobalNetworkConfig());
  ASSERT_TRUE(profile_policies.GetGlobalNetworkConfig()->is_dict());
  EXPECT_TRUE(profile_policies.GetGlobalNetworkConfig()->DictEmpty());
}

// Sets / retrieves GlobalNetworkConfig.
TEST_F(ProfilePoliciesTest, SetAndOverwriteGlobalNetworkConfig) {
  base::Value global_network_config_1(base::Value::Type::DICTIONARY);
  global_network_config_1.SetKey("key1", base::Value("value1"));

  base::Value global_network_config_2(base::Value::Type::DICTIONARY);
  global_network_config_2.SetKey("key2", base::Value("value2"));

  ProfilePolicies profile_policies;
  profile_policies.SetGlobalNetworkConfig(global_network_config_1);
  ASSERT_TRUE(profile_policies.GetGlobalNetworkConfig());
  EXPECT_EQ(*profile_policies.GetGlobalNetworkConfig(),
            global_network_config_1);

  profile_policies.SetGlobalNetworkConfig(global_network_config_2);
  ASSERT_TRUE(profile_policies.GetGlobalNetworkConfig());
  EXPECT_EQ(*profile_policies.GetGlobalNetworkConfig(),
            global_network_config_2);
}

// Tests accessors to per-network policies when no policy has been set.
TEST_F(ProfilePoliciesTest, NoNetworkPolicy) {
  ProfilePolicies profile_policies;
  EXPECT_EQ(profile_policies.GetPolicyByGuid("guid"), nullptr);
  EXPECT_THAT(profile_policies.GetAllPolicyGuids(), IsEmpty());
  EXPECT_THAT(profile_policies.GetGuidToPolicyMap(), IsEmpty());
}

// Applies a network policy and checks accessors to per-network policies.
// The original policy (none) had no network configs.
// The new poilcy has no network configs.
TEST_F(ProfilePoliciesTest, ApplyOncNetworkConfigurationListZeroToZero) {
  base::Value network_configs(base::Value::Type::LIST);

  ProfilePolicies profile_policies;
  base::flat_set<std::string> new_or_modified_guids =
      profile_policies.ApplyOncNetworkConfigurationList(network_configs);
  EXPECT_THAT(new_or_modified_guids, IsEmpty());

  EXPECT_THAT(profile_policies.GetAllPolicyGuids(), IsEmpty());
  EXPECT_THAT(profile_policies.GetGuidToPolicyMap(), IsEmpty());
}

// Applies a network policy and checks accessors to per-network policies.
// The original policy (none) had no network configs.
// The new poilcy has one network config.
TEST_F(ProfilePoliciesTest, ApplyOncNetworkConfigurationListZeroToOne) {
  base::Value network_config_1 = NetworkConfig("guid1");
  base::Value network_configs = NetworkConfigsList({&network_config_1});

  ProfilePolicies profile_policies;
  base::flat_set<std::string> new_or_modified_guids =
      profile_policies.ApplyOncNetworkConfigurationList(network_configs);
  EXPECT_THAT(new_or_modified_guids, ElementsAre("guid1"));

  EXPECT_THAT(profile_policies.GetAllPolicyGuids(), ElementsAre("guid1"));

  ASSERT_TRUE(profile_policies.GetPolicyByGuid("guid1"));
  EXPECT_EQ(*profile_policies.GetPolicyByGuid("guid1"), network_config_1);
  EXPECT_THAT(profile_policies.GetGuidToPolicyMap(),
              ElementsAre(Pair("guid1", base::test::IsJson(network_config_1))));
}

// Applies a network policy and checks accessors to per-network policies.
// Goes from no policy (0 networks) -> policy with 1 network -> policy with 0
// networks.
TEST_F(ProfilePoliciesTest, ApplyOncNetworkConfigurationListOneToZero) {
  base::Value network_config_1 = NetworkConfig("guid1");
  base::Value network_configs_orig = NetworkConfigsList({&network_config_1});

  ProfilePolicies profile_policies;
  profile_policies.ApplyOncNetworkConfigurationList(network_configs_orig);
  EXPECT_THAT(profile_policies.GetAllPolicyGuids(), ElementsAre("guid1"));

  base::Value network_configs_new = NetworkConfigsList({});
  base::flat_set<std::string> new_or_modified_guids =
      profile_policies.ApplyOncNetworkConfigurationList(network_configs_new);
  EXPECT_THAT(new_or_modified_guids, IsEmpty());

  EXPECT_THAT(profile_policies.GetAllPolicyGuids(), IsEmpty());
  EXPECT_THAT(profile_policies.GetGuidToPolicyMap(), IsEmpty());
}

// Applies a network policy and checks accessors to per-network policies.
// Tests re-application of exactly the same policy (no effective change).
TEST_F(ProfilePoliciesTest, ApplyOncNetworkConfigurationListNoChange) {
  base::Value network_config_1 = NetworkConfig("guid1");
  base::Value network_configs_orig = NetworkConfigsList({&network_config_1});

  ProfilePolicies profile_policies;
  profile_policies.ApplyOncNetworkConfigurationList(network_configs_orig);
  EXPECT_THAT(profile_policies.GetAllPolicyGuids(), ElementsAre("guid1"));

  base::flat_set<std::string> new_or_modified_guids =
      profile_policies.ApplyOncNetworkConfigurationList(network_configs_orig);
  EXPECT_THAT(new_or_modified_guids, IsEmpty());

  EXPECT_THAT(profile_policies.GetAllPolicyGuids(), ElementsAre("guid1"));
}

// Applies a network policy and checks accessors to per-network policies.
// Applies a policy with a network config "guid1".
// Applies another policy where "guid1" has changed contents.
// Tests that "guid1" is reported as changed and has the new contents.
TEST_F(ProfilePoliciesTest, ApplyOncNetworkConfigurationListChange) {
  base::Value network_config_orig = NetworkConfig("guid1");
  base::Value network_configs_orig = NetworkConfigsList({&network_config_orig});

  ProfilePolicies profile_policies;
  profile_policies.ApplyOncNetworkConfigurationList(network_configs_orig);
  ASSERT_TRUE(profile_policies.GetPolicyByGuid("guid1"));
  EXPECT_EQ(*profile_policies.GetPolicyByGuid("guid1"), network_config_orig);

  base::Value network_config_changed = network_config_orig.Clone();
  network_config_changed.SetKey("changed", base::Value("changed"));
  base::Value network_configs_changed =
      NetworkConfigsList({&network_config_changed});

  base::flat_set<std::string> new_or_modified_guids =
      profile_policies.ApplyOncNetworkConfigurationList(
          network_configs_changed);
  EXPECT_THAT(profile_policies.GetAllPolicyGuids(), ElementsAre("guid1"));

  EXPECT_THAT(profile_policies.GetAllPolicyGuids(), ElementsAre("guid1"));
  ASSERT_TRUE(profile_policies.GetPolicyByGuid("guid1"));
  EXPECT_NE(*profile_policies.GetPolicyByGuid("guid1"), network_config_orig);
  EXPECT_EQ(*profile_policies.GetPolicyByGuid("guid1"), network_config_changed);
}

// Applies a network policy with multiple NetworkConfiguration elements and
// checks accessors to per-network policies.
TEST_F(ProfilePoliciesTest, MultipleElemetns) {
  base::Value network_config_1 = NetworkConfig("guid1");
  network_config_1.SetKey("test1", base::Value("value1"));
  base::Value network_config_2 = NetworkConfig("guid2");
  network_config_2.SetKey("test2", base::Value("value2"));
  base::Value network_configs_orig =
      NetworkConfigsList({&network_config_1, &network_config_2});

  ProfilePolicies profile_policies;
  profile_policies.ApplyOncNetworkConfigurationList(network_configs_orig);
  EXPECT_THAT(profile_policies.GetAllPolicyGuids(),
              UnorderedElementsAre("guid1", "guid2"));
  EXPECT_THAT(profile_policies.GetGuidToPolicyMap(),
              UnorderedElementsAre(
                  Pair("guid1", base::test::IsJson(network_config_1)),
                  Pair("guid2", base::test::IsJson(network_config_2))));
}

// Tests HasPolicyMatchingShillProperties for the case that no policy matches.
TEST_F(ProfilePoliciesTest, HasPolicyMatchingShillPropertiesNoMatch) {
  base::Value network_config_1 = NetworkConfig("guid1");
  base::Value network_configs_orig = NetworkConfigsList({&network_config_1});

  ProfilePolicies profile_policies;
  profile_policies.SetShillPropertiesMatcherForTesting(
      base::BindRepeating(&FalseShillPropertiesMatcher));
  profile_policies.ApplyOncNetworkConfigurationList(network_configs_orig);

  EXPECT_FALSE(
      profile_policies.HasPolicyMatchingShillProperties(base::Value()));
}

// Tests HasPolicyMatchingShillProperties for the case that a policy matches.
TEST_F(ProfilePoliciesTest, HasPolicyMatchingShillPropertiesMatch) {
  base::Value network_config_1 = NetworkConfig("guid1");
  base::Value network_config_2 = NetworkConfig("guid2");
  network_config_2.SetKey("marker", base::Value("value"));
  base::Value network_configs_orig =
      NetworkConfigsList({&network_config_1, &network_config_2});

  ProfilePolicies profile_policies;
  profile_policies.SetShillPropertiesMatcherForTesting(
      base::BindRepeating(&EqShillPropertiesMatcher));
  profile_policies.ApplyOncNetworkConfigurationList(network_configs_orig);

  EXPECT_TRUE(
      profile_policies.HasPolicyMatchingShillProperties(network_config_2));
}

}  // namespace
}  // namespace chromeos
