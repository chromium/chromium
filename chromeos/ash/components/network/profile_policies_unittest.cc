// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/profile_policies.h"

#include <string_view>

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chromeos/ash/components/network/client_cert_util.h"
#include "components/onc/onc_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

// Creates a list containing clones of elements passed in the initializer list.
base::Value::List NetworkConfigsList(
    std::initializer_list<const base::Value::Dict*> network_configs) {
  base::Value::List result;
  for (const auto* network_config : network_configs) {
    result.Append(network_config->Clone());
  }
  return result;
}

// Creates a very basic for-testing NetworkConfig, essentially
// {
//   "guid": <passed_guid>
// }
base::Value::Dict NetworkConfig(std::string_view guid) {
  return base::Value::Dict().Set(::onc::network_config::kGUID, guid);
}

bool FalseShillPropertiesMatcher(
    const base::Value::Dict& onc_network_configuration,
    const base::Value::Dict& shill_properties) {
  return false;
}

bool EqShillPropertiesMatcher(
    const base::Value::Dict& onc_network_configuration,
    const base::Value::Dict& shill_properties) {
  return onc_network_configuration == shill_properties;
}

// A runtime values setter which doesn't change its input ONC dictionary.
// Useful for emulating that setting runtime values resulted in no change from
// the "original" ONC dictionary.
base::Value::Dict NoOp(
    const base::Value::Dict& onc_network_configuration,
    const base::flat_map<std::string, std::string>& profile_wide_expansions,
    const client_cert::ResolvedCert& resolved_cert) {
  return onc_network_configuration.Clone();
}

// A RuntimeValuesSetter which injects its inputs as
// dictionary keys. Useful for checking behavior when variable
// setting runtime values actually changes the ONC dictionary, and for easy
// verification of the values that have been passed to the RuntimeValuesSetter.
base::Value::Dict Inject(
    const base::Value::Dict& onc_network_configuration,
    const base::flat_map<std::string, std::string>& profile_wide_expansions,
    const client_cert::ResolvedCert& resolved_cert) {
  base::Value::Dict result = onc_network_configuration.Clone();

  base::Value::Dict profile_wide_expansions_dict;
  for (const auto& pair : profile_wide_expansions) {
    profile_wide_expansions_dict.Set(pair.first, pair.second);
  }
  result.Set("profile_wide_expansions",
             std::move(profile_wide_expansions_dict));

  if (resolved_cert.status() ==
      client_cert::ResolvedCert::Status::kNothingMatched) {
    result.Set("cert_info", base::Value::Dict().Set("status", "no cert"));
  } else if (resolved_cert.status() ==
             client_cert::ResolvedCert::Status::kCertMatched) {
    auto cert_dict = base::Value::Dict()
                         .Set("slot_id", resolved_cert.slot_id())
                         .Set("pkcs11_id", resolved_cert.pkcs11_id());
    for (const auto& pair : resolved_cert.variable_expansions()) {
      cert_dict.Set(pair.first, pair.second);
    }

    result.Set("cert_info", std::move(cert_dict));
  }

  return result;
}

}  // namespace

using ProfilePoliciesTest = ::testing::Test;

// Calls GetGlobalNetworkConfig when no GlobalNetworkConfig has been set yet.
TEST(ProfilePoliciesTest, GlobalNetworkConfigIsEmpty) {
  ProfilePolicies profile_policies;
  ASSERT_TRUE(profile_policies.GetGlobalNetworkConfig());
  EXPECT_TRUE(profile_policies.GetGlobalNetworkConfig()->empty());
}

// Sets / retrieves GlobalNetworkConfig.
TEST(ProfilePoliciesTest, SetAndOverwriteGlobalNetworkConfig) {
  auto global_network_config_1 = base::Value::Dict().Set("key1", "value1");
  auto global_network_config_2 = base::Value::Dict().Set("key2", "value2");

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
TEST(ProfilePoliciesTest, NoNetworkPolicy) {
  ProfilePolicies profile_policies;
  EXPECT_EQ(profile_policies.GetPolicyByGuid("guid"), nullptr);
  EXPECT_THAT(profile_policies.GetAllPolicyGuids(), IsEmpty());
  EXPECT_THAT(profile_policies.GetGuidToPolicyMap(), IsEmpty());
}

// Applies a network policy and checks accessors to per-network policies.
// The original policy (none) had no network configs.
// The new policy has no network configs.
TEST(ProfilePoliciesTest, ApplyOncNetworkConfigurationListZeroToZero) {
  base::Value::List network_configs;

  ProfilePolicies profile_policies;
  base::flat_set<std::string> new_or_modified_guids =
      profile_policies.ApplyOncNetworkConfigurationList(network_configs);
  EXPECT_THAT(new_or_modified_guids, IsEmpty());

  EXPECT_THAT(profile_policies.GetAllPolicyGuids(), IsEmpty());
  EXPECT_THAT(profile_policies.GetGuidToPolicyMap(), IsEmpty());
}

// Applies a network policy and checks accessors to per-network policies.
// The original policy (none) had no network configs.
// The new policy has one network config.
TEST(ProfilePoliciesTest, ApplyOncNetworkConfigurationListZeroToOne) {
  base::Value::Dict network_config_1 = NetworkConfig("guid1");
  base::Value::List network_configs = NetworkConfigsList({&network_config_1});

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
TEST(ProfilePoliciesTest, ApplyOncNetworkConfigurationListOneToZero) {
  base::Value::Dict network_config_1 = NetworkConfig("guid1");
  base::Value::List network_configs_orig =
      NetworkConfigsList({&network_config_1});

  ProfilePolicies profile_policies;
  profile_policies.ApplyOncNetworkConfigurationList(network_configs_orig);
  EXPECT_THAT(profile_policies.GetAllPolicyGuids(), ElementsAre("guid1"));

  base::Value::List network_configs_new = NetworkConfigsList({});
  base::flat_set<std::string> new_or_modified_guids =
      profile_policies.ApplyOncNetworkConfigurationList(network_configs_new);
  EXPECT_THAT(new_or_modified_guids, IsEmpty());

  EXPECT_THAT(profile_policies.GetAllPolicyGuids(), IsEmpty());
  EXPECT_THAT(profile_policies.GetGuidToPolicyMap(), IsEmpty());
}

// Applies a network policy and checks accessors to per-network policies.
// Tests re-application of exactly the same policy (no effective change).
TEST(ProfilePoliciesTest, ApplyOncNetworkConfigurationListNoChange) {
  base::Value::Dict network_config_1 = NetworkConfig("guid1");
  base::Value::List network_configs_orig =
      NetworkConfigsList({&network_config_1});

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
TEST(ProfilePoliciesTest, ApplyOncNetworkConfigurationListChange) {
  base::Value::Dict network_config_orig = NetworkConfig("guid1");
  base::Value::List network_configs_orig =
      NetworkConfigsList({&network_config_orig});

  ProfilePolicies profile_policies;
  profile_policies.ApplyOncNetworkConfigurationList(network_configs_orig);
  ASSERT_TRUE(profile_policies.GetPolicyByGuid("guid1"));
  EXPECT_EQ(*profile_policies.GetPolicyByGuid("guid1"), network_config_orig);

  base::Value::Dict network_config_changed = network_config_orig.Clone();
  network_config_changed.Set("changed", "changed");
  base::Value::List network_configs_changed =
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
TEST(ProfilePoliciesTest, MultipleElements) {
  base::Value::Dict network_config_1 = NetworkConfig("guid1");
  network_config_1.Set("test1", "value1");
  base::Value::Dict network_config_2 = NetworkConfig("guid2");
  network_config_2.Set("test2", "value2");
  base::Value::List network_configs_orig =
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
TEST(ProfilePoliciesTest, HasPolicyMatchingShillPropertiesNoMatch) {
  base::Value::Dict network_config_1 = NetworkConfig("guid1");
  base::Value::List network_configs_orig =
      NetworkConfigsList({&network_config_1});

  ProfilePolicies profile_policies;
  profile_policies.SetShillPropertiesMatcherForTesting(
      base::BindRepeating(&FalseShillPropertiesMatcher));
  profile_policies.ApplyOncNetworkConfigurationList(network_configs_orig);

  EXPECT_FALSE(
      profile_policies.HasPolicyMatchingShillProperties(base::Value::Dict()));
}

// Tests HasPolicyMatchingShillProperties for the case that a policy matches.
TEST(ProfilePoliciesTest, HasPolicyMatchingShillPropertiesMatch) {
  base::Value::Dict network_config_1 = NetworkConfig("guid1");
  base::Value::Dict network_config_2 = NetworkConfig("guid2");
  network_config_2.Set("marker", "value");
  base::Value::List network_configs_orig =
      NetworkConfigsList({&network_config_1, &network_config_2});

  ProfilePolicies profile_policies;
  profile_policies.SetShillPropertiesMatcherForTesting(
      base::BindRepeating(&EqShillPropertiesMatcher));
  profile_policies.ApplyOncNetworkConfigurationList(network_configs_orig);

  EXPECT_TRUE(
      profile_policies.HasPolicyMatchingShillProperties(network_config_2));
}

// Tests that profile-wide expansions apply if they were configured before the
// NetworkConfiguration was applied.
TEST(ProfilePoliciesTest, ProfileWideExpansionsAlreadyExist) {
  base::Value::Dict network_config = NetworkConfig("guid1");
  base::Value::List network_configs = NetworkConfigsList({&network_config});

  ProfilePolicies profile_policies;

  // Replace the variable expander with one which is simply injecting all
  // expansions as top-level dictionary keys instead.
  profile_policies.SetRuntimeValuesSetterForTesting(
      base::BindRepeating(&Inject));

  {
    base::flat_set<std::string> modified_guids =
        profile_policies.SetProfileWideExpansions(
            {{"profileWideVar", "profileWideValue"}});
    EXPECT_THAT(modified_guids, IsEmpty());
  }

  profile_policies.ApplyOncNetworkConfigurationList(network_configs);

  EXPECT_THAT(profile_policies.GetGuidToPolicyMap(),
              ElementsAre(Pair("guid1", base::test::IsJson(R"(
                      {
                        "GUID": "guid1",
                        "profile_wide_expansions": {
                          "profileWideVar": "profileWideValue"
                        }
                      })"))));
}

// Tests that nothing happens when the variable expansions change if no
// NetworkConfiguration depends on them. This is emulated by using the NoOp
// expander (in reality it would happen if no network configuration contains a
// known expansion).
TEST(ProfilePoliciesTest, ChangeNoEffect) {
  base::Value::Dict network_config_1 = NetworkConfig("guid1");
  base::Value::List network_configs = NetworkConfigsList({&network_config_1});

  ProfilePolicies profile_policies;
  profile_policies.SetRuntimeValuesSetterForTesting(base::BindRepeating(&NoOp));
  profile_policies.ApplyOncNetworkConfigurationList(network_configs);

  base::flat_set<std::string> modified_guids =
      profile_policies.SetProfileWideExpansions(
          {{"profileWideVar", "profileWideValue"}});
  EXPECT_THAT(modified_guids, IsEmpty());
  EXPECT_THAT(profile_policies.GetGuidToPolicyMap(),
              UnorderedElementsAre(
                  Pair("guid1", base::test::IsJson(network_config_1))));
}

// Tests that when variable expansions (both profile-wide and network-specific)
// change and a NetworkConfiguration depends on them, the policy value with
// expansions is updated accordingly and the setter for the expansions returns
// back the set of affected NetworkConfiguration GUIDs to its caller.
TEST(ProfilePoliciesTest, ExpansionsChangeAffectsNetworkConfiguration) {
  base::Value::Dict network_config_1 = NetworkConfig("guid1");
  base::Value::Dict network_config_2 = NetworkConfig("guid2");
  base::Value::List network_configs =
      NetworkConfigsList({&network_config_1, &network_config_2});

  ProfilePolicies profile_policies;

  // Replace the variable expander with one which is simply injecting all
  // expansions as top-level dictionary keys instead.
  profile_policies.SetRuntimeValuesSetterForTesting(
      base::BindRepeating(&Inject));

  profile_policies.ApplyOncNetworkConfigurationList(network_configs);

  {
    base::flat_set<std::string> modified_guids =
        profile_policies.SetProfileWideExpansions(
            {{"profileWideVar", "profileWideValue"}});
    EXPECT_THAT(modified_guids, UnorderedElementsAre("guid1", "guid2"));
  }

  EXPECT_THAT(profile_policies.GetGuidToPolicyMap(),
              UnorderedElementsAre(Pair("guid1", base::test::IsJson(R"(
                      {
                        "GUID": "guid1",
                        "profile_wide_expansions": {
                          "profileWideVar": "profileWideValue"
                        }
                      })")),
                                   Pair("guid2", base::test::IsJson(R"(
                      {
                        "GUID": "guid2",
                        "profile_wide_expansions": {
                          "profileWideVar": "profileWideValue"
                        }
                      })"))));

  {
    bool change_had_effect = profile_policies.SetResolvedClientCertificate(
        "guid1", client_cert::ResolvedCert::CertMatched(
                     1, "pkcs11_id", {{"certVar", "certVarValue"}}));
    EXPECT_TRUE(change_had_effect);
  }

  EXPECT_THAT(profile_policies.GetGuidToPolicyMap(),
              UnorderedElementsAre(Pair("guid1", base::test::IsJson(R"(
                        {
                          "GUID": "guid1",
                          "profile_wide_expansions": {
                            "profileWideVar": "profileWideValue"
                          },
                          "cert_info": {
                            "slot_id": 1,
                            "pkcs11_id": "pkcs11_id",
                            "certVar": "certVarValue"
                          }
                        })")),
                                   Pair("guid2", base::test::IsJson(R"(
                        {
                          "GUID": "guid2",
                          "profile_wide_expansions": {
                            "profileWideVar": "profileWideValue"
                          }
                        })"))));

  {
    bool change_had_effect = profile_policies.SetResolvedClientCertificate(
        "guid1", client_cert::ResolvedCert::NothingMatched());
    EXPECT_TRUE(change_had_effect);
  }

  EXPECT_THAT(profile_policies.GetGuidToPolicyMap(),
              UnorderedElementsAre(Pair("guid1", base::test::IsJson(R"(
                        {
                          "GUID": "guid1",
                          "profile_wide_expansions": {
                            "profileWideVar": "profileWideValue"
                          },
                          "cert_info": {
                            "status": "no cert"
                          }
                        })")),
                                   Pair("guid2", base::test::IsJson(R"(
                        {
                          "GUID": "guid2",
                          "profile_wide_expansions": {
                            "profileWideVar": "profileWideValue"
                          }
                        })"))));

  {
    base::flat_set<std::string> modified_guids =
        profile_policies.SetProfileWideExpansions({});
    EXPECT_THAT(modified_guids, UnorderedElementsAre("guid1", "guid2"));
  }

  EXPECT_THAT(profile_policies.GetGuidToPolicyMap(),
              UnorderedElementsAre(Pair("guid1", base::test::IsJson(R"(
                        {
                          "GUID": "guid1",
                          "profile_wide_expansions": {},
                          "cert_info": {
                            "status": "no cert"
                          }
                        })")),
                                   Pair("guid2", base::test::IsJson(R"(
                        {
                          "GUID": "guid2",
                          "profile_wide_expansions": {}
                        })"))));
}

// Tests that variable expansions (both profile-wide and per-network) are
// applied when a NetworkConfiguration changes.
TEST(ProfilePoliciesTest, NetworkConfigurationChangeWithExistingExpansions) {
  ProfilePolicies profile_policies;

  // Replace the variable expander with one which is simply injecting all
  // expansions as top-level dictionary keys instead.
  profile_policies.SetRuntimeValuesSetterForTesting(
      base::BindRepeating(&Inject));

  {
    base::flat_set<std::string> modified_guids =
        profile_policies.SetProfileWideExpansions(
            {{"profileWideVar", "profileWideValue"}});
    EXPECT_THAT(modified_guids, IsEmpty());
  }

  {
    base::Value::Dict network_config_1 = NetworkConfig("guid1");
    base::Value::List network_configs = NetworkConfigsList({&network_config_1});
    base::flat_set<std::string> modified_guids =
        profile_policies.ApplyOncNetworkConfigurationList(network_configs);
    EXPECT_THAT(modified_guids, UnorderedElementsAre("guid1"));
  }

  {
    bool change_had_effect = profile_policies.SetResolvedClientCertificate(
        "guid1", client_cert::ResolvedCert::CertMatched(
                     1, "pkcs11_id", {{"certVar", "certVarValue"}}));
    EXPECT_TRUE(change_had_effect);
  }
  EXPECT_THAT(profile_policies.GetGuidToPolicyMap(),
              UnorderedElementsAre(Pair("guid1", base::test::IsJson(R"(
                      {
                        "GUID": "guid1",
                        "profile_wide_expansions": {
                          "profileWideVar": "profileWideValue"
                        },
                        "cert_info": {
                          "slot_id": 1,
                          "pkcs11_id": "pkcs11_id",
                          "certVar": "certVarValue"
                        }
                      })"))));
  {
    base::Value::Dict network_config_1 = NetworkConfig("guid1");
    network_config_1.Set("modified", "yes");
    base::Value::List network_configs = NetworkConfigsList({&network_config_1});
    base::flat_set<std::string> modified_guids =
        profile_policies.ApplyOncNetworkConfigurationList(network_configs);
    EXPECT_THAT(modified_guids, UnorderedElementsAre("guid1"));
  }
  EXPECT_THAT(profile_policies.GetGuidToPolicyMap(),
              UnorderedElementsAre(Pair("guid1", base::test::IsJson(R"(
                      {
                        "GUID": "guid1",
                        "modified": "yes",
                        "profile_wide_expansions": {
                          "profileWideVar": "profileWideValue"
                        },
                        "cert_info": {
                          "slot_id": 1,
                          "pkcs11_id": "pkcs11_id",
                          "certVar": "certVarValue"
                        }
                      })"))));
}

// Tests that GetOriginalPolicyByGuid returns the policy without
// variable expansions.
TEST(ProfilePoliciesTest, GetOriginalPolicyByGuid) {
  base::Value::Dict network_config_1 = NetworkConfig("guid1");
  base::Value::List network_configs = NetworkConfigsList({&network_config_1});

  ProfilePolicies profile_policies;

  // Replace the variable expander with one which is simply injecting all
  // expansions as top-level dictionary keys instead.
  profile_policies.SetRuntimeValuesSetterForTesting(
      base::BindRepeating(&Inject));

  profile_policies.ApplyOncNetworkConfigurationList(network_configs);

  profile_policies.SetProfileWideExpansions(
      {{"profileWideVar", "profileWideValue"}});
  const base::Value::Dict* policy_with_expansions =
      profile_policies.GetPolicyByGuid("guid1");
  const base::Value::Dict* policy_without_expansions =
      profile_policies.GetOriginalPolicyByGuid("guid1");
  ASSERT_TRUE(policy_with_expansions);
  ASSERT_TRUE(policy_without_expansions);

  EXPECT_THAT(*policy_with_expansions, base::test::IsJson(R"(
                    {
                      "GUID": "guid1",
                      "profile_wide_expansions": {
                        "profileWideVar": "profileWideValue"
                      }
                    })"));
  EXPECT_THAT(*policy_without_expansions, base::test::IsJson(R"(
                    {
                      "GUID": "guid1"
                    })"));
}

// Tests that setting per-network variable expansions doesn't do anything (and
// doesn't crash) if the GUID is unknown.
TEST(ProfilePoliciesTest, SetResolvedClientCertificateUnknownGuid) {
  ProfilePolicies profile_policies;

  // Replace the variable expander with one which is simply injecting all
  // expansions as top-level dictionary keys instead.
  profile_policies.SetRuntimeValuesSetterForTesting(
      base::BindRepeating(&Inject));

  bool change_had_effect = profile_policies.SetResolvedClientCertificate(
      "unknown_guid", client_cert::ResolvedCert::CertMatched(
                          1, "pkcs11_id", {{"var1", "value1"}}));
  EXPECT_FALSE(change_had_effect);
}

}  // namespace ash
