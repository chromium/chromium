// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/lookalikes/core/flat_safety_tips_allowlist.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "components/lookalikes/core/safety_tip_test_utils.h"
#include "components/lookalikes/core/safety_tips.pb.h"
#include "components/lookalikes/core/safety_tips_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace lookalikes {

namespace {

void AddAllowedPattern(reputation::SafetyTipsConfig& config,
                       const std::string& pattern,
                       const std::vector<uint32_t>& cohort_indices) {
  auto* allowed_pattern = config.add_allowed_pattern();
  allowed_pattern->set_pattern(pattern);
  for (const uint32_t index : cohort_indices) {
    allowed_pattern->add_cohort_index(index);
  }
}

void AddCohort(reputation::SafetyTipsConfig& config,
               const std::vector<uint32_t>& allowed_indices,
               const std::vector<uint32_t>& canonical_indices) {
  auto* cohort = config.add_cohort();
  for (const uint32_t index : allowed_indices) {
    cohort->add_allowed_index(index);
  }
  for (const uint32_t index : canonical_indices) {
    cohort->add_canonical_index(index);
  }
}

// Returns a config with all fields set, whose allowlist covers each case of
// the lookup.
reputation::SafetyTipsConfig CreateConfig() {
  reputation::SafetyTipsConfig config;
  config.set_version_id(4);
  config.add_flagged_page()->set_pattern("flagged.tld/");
  config.add_allowed_target_pattern()->set_regex("target\\.tld");
  config.add_common_word("common");
  config.add_launch_config()->set_launch_percentage(50);

  // `allowed_pattern` must stay sorted. The first three entries refer to a
  // cohort with an invalid allowed index, a cohort with an invalid canonical
  // index, and an invalid cohort.
  AddAllowedPattern(config, "error-allowed-index.tld/", {0});
  AddAllowedPattern(config, "error-canonical-index.tld/", {1});
  AddAllowedPattern(config, "error-cohort-index.tld/", {100});
  AddAllowedPattern(config, "siteb.tld/", {2});
  AddAllowedPattern(config, "sitec.tld/", {3});
  // Entries without cohorts may spoof any site.
  AddAllowedPattern(config, "sited.tld/", {});
  AddAllowedPattern(config, "sitef.tld/path/", {});
  // Only the first of equal entries counts, so sitez.tld may only spoof
  // itself.
  AddAllowedPattern(config, "sitez.tld/", {4});
  AddAllowedPattern(config, "sitez.tld/", {});

  config.add_canonical_pattern()->set_pattern("sitea.tld/");

  AddCohort(config, /*allowed_indices=*/{100}, /*canonical_indices=*/{});
  AddCohort(config, /*allowed_indices=*/{}, /*canonical_indices=*/{100});
  // siteb.tld and sitea.tld.
  AddCohort(config, /*allowed_indices=*/{3}, /*canonical_indices=*/{0});
  // siteb.tld and sitec.tld.
  AddCohort(config, /*allowed_indices=*/{3, 4}, /*canonical_indices=*/{});
  // sitez.tld.
  AddCohort(config, /*allowed_indices=*/{7}, /*canonical_indices=*/{});
  return config;
}

// Installs a copy of `config` with its allowlist extracted, like the Safety
// Tips component installer.
void InstallWithAllowlist(const reputation::SafetyTipsConfig& config) {
  auto installed = std::make_unique<reputation::SafetyTipsConfig>(config);
  std::unique_ptr<FlatSafetyTipsAllowlist> allowlist =
      FlatSafetyTipsAllowlist::ExtractFrom(*installed);
  ASSERT_TRUE(allowlist);
  SetSafetyTipsRemoteConfig(std::move(installed), std::move(allowlist));
}

// Returns whether the installed config allows `visited` to spoof `canonical`.
bool IsAllowlisted(const std::string& visited, const std::string& canonical) {
  return IsUrlAllowlistedBySafetyTipsComponent(GetSafetyTipsRemoteConfigProto(),
                                               GURL(visited), GURL(canonical));
}

}  // namespace

class FlatSafetyTipsAllowlistTest : public testing::Test {
 protected:
  void TearDown() override { SetSafetyTipsRemoteConfigProto(nullptr); }
};

// Lookups on the extracted allowlist return the same results as on the
// original config.
TEST_F(FlatSafetyTipsAllowlistTest, MatchesProtoLookup) {
  const reputation::SafetyTipsConfig reference = CreateConfig();
  InstallWithAllowlist(reference);
  const reputation::SafetyTipsConfig* installed =
      GetSafetyTipsRemoteConfigProto();
  ASSERT_TRUE(installed);
  // The installed config no longer holds the allowlist, so its lookups use the
  // extracted one. `reference` isn't the installed config, so it is looked up
  // as a proto.
  EXPECT_EQ(0, installed->allowed_pattern_size());

  const std::vector<GURL> urls = {
      GURL("http://sitea.tld/"),
      GURL("http://siteb.tld/"),
      GURL("http://sub.siteb.tld/"),
      GURL("http://sitec.tld/"),
      GURL("http://sited.tld/"),
      GURL("http://sitee.tld/"),
      GURL("http://sitef.tld/path/x"),
      GURL("http://sitef.tld/other"),
      GURL("http://sitez.tld/"),
      GURL("http://error-allowed-index.tld/"),
      GURL("http://error-canonical-index.tld/"),
      GURL("http://error-cohort-index.tld/"),
  };
  for (const GURL& visited : urls) {
    for (const GURL& canonical : urls) {
      SCOPED_TRACE(visited.spec() + " spoofing " + canonical.spec());
      const bool from_proto =
          IsUrlAllowlistedBySafetyTipsComponent(&reference, visited, canonical);
      const bool from_flat =
          IsUrlAllowlistedBySafetyTipsComponent(installed, visited, canonical);
      EXPECT_EQ(from_proto, from_flat);
    }
  }

  // Makes sure that the comparison above covers both results.
  EXPECT_TRUE(IsAllowlisted("http://siteb.tld/", "http://sitea.tld/"));
  EXPECT_TRUE(IsAllowlisted("http://sub.siteb.tld/", "http://sitea.tld/"));
  EXPECT_FALSE(IsAllowlisted("http://sitec.tld/", "http://sitea.tld/"));
  EXPECT_TRUE(IsAllowlisted("http://sitec.tld/", "http://siteb.tld/"));
  EXPECT_TRUE(IsAllowlisted("http://sited.tld/", "http://sitee.tld/"));
  EXPECT_TRUE(IsAllowlisted("http://sitef.tld/path/x", "http://sitee.tld/"));
  EXPECT_FALSE(IsAllowlisted("http://sitef.tld/other", "http://sitee.tld/"));
  EXPECT_TRUE(IsAllowlisted("http://sitez.tld/", "http://sitez.tld/"));
  EXPECT_FALSE(IsAllowlisted("http://sitez.tld/", "http://sitea.tld/"));
  EXPECT_FALSE(
      IsAllowlisted("http://error-cohort-index.tld/", "http://sitea.tld/"));
}

// Only the allowlist fields are taken from the config, and their memory is
// freed.
TEST_F(FlatSafetyTipsAllowlistTest, ExtractFromFreesOnlyAllowlistFields) {
  reputation::SafetyTipsConfig config = CreateConfig();
  reputation::SafetyTipsConfig expected = config;
  expected.clear_allowed_pattern();
  expected.clear_canonical_pattern();
  expected.clear_cohort();

  ASSERT_TRUE(FlatSafetyTipsAllowlist::ExtractFrom(config));
  EXPECT_EQ(expected.SerializeAsString(), config.SerializeAsString());

  // Clearing a repeated field would keep its elements allocated for reuse.
  const reputation::SafetyTipsConfig empty;
  EXPECT_EQ(empty.allowed_pattern().Capacity(),
            config.allowed_pattern().Capacity());
  EXPECT_EQ(empty.canonical_pattern().Capacity(),
            config.canonical_pattern().Capacity());
  EXPECT_EQ(empty.cohort().Capacity(), config.cohort().Capacity());
}

// The fields left in the installed config are still used.
TEST_F(FlatSafetyTipsAllowlistTest, InstalledConfigKeepsOtherFields) {
  InstallWithAllowlist(CreateConfig());
  const reputation::SafetyTipsConfig* installed =
      GetSafetyTipsRemoteConfigProto();
  ASSERT_TRUE(installed);
  EXPECT_TRUE(
      IsTargetHostAllowlistedBySafetyTipsComponent(installed, "target.tld"));
  EXPECT_TRUE(IsCommonWordInConfigProto(installed, "common"));
}

// The canonical URL patterns are computed at most once, and only if a matching
// entry refers to a valid cohort.
TEST_F(FlatSafetyTipsAllowlistTest, ComputesCanonicalPatternsOnlyWhenNeeded) {
  reputation::SafetyTipsConfig config = CreateConfig();
  std::unique_ptr<FlatSafetyTipsAllowlist> allowlist =
      FlatSafetyTipsAllowlist::ExtractFrom(config);
  ASSERT_TRUE(allowlist);

  int calls = 0;
  auto is_allowlisted = [&](const std::vector<std::string>& patterns) {
    return allowlist->IsUrlAllowlisted(patterns, [&calls] {
      ++calls;
      return std::vector<std::string>{"sitea.tld/"};
    });
  };
  // No entry matches.
  EXPECT_FALSE(is_allowlisted({"sitee.tld/"}));
  // The entry may spoof any site.
  EXPECT_TRUE(is_allowlisted({"sited.tld/"}));
  // The entry only refers to an invalid cohort.
  EXPECT_FALSE(is_allowlisted({"error-cohort-index.tld/"}));
  EXPECT_EQ(0, calls);

  // Both entries refer to cohorts, but only the second one's includes
  // sitea.tld.
  EXPECT_TRUE(is_allowlisted({"sitec.tld/", "siteb.tld/"}));
  EXPECT_EQ(1, calls);
}

// SetSafetyTipsRemoteConfigProto() moves the allowlist out of the config too,
// and replacing the config replaces its allowlist.
TEST_F(FlatSafetyTipsAllowlistTest, SetConfigProtoExtractsAllowlist) {
  SetSafetyTipsRemoteConfigProto(
      std::make_unique<reputation::SafetyTipsConfig>(CreateConfig()));
  const reputation::SafetyTipsConfig* installed =
      GetSafetyTipsRemoteConfigProto();
  ASSERT_TRUE(installed);
  EXPECT_EQ(0, installed->allowed_pattern_size());
  EXPECT_EQ(0, installed->canonical_pattern_size());
  EXPECT_EQ(0, installed->cohort_size());
  EXPECT_TRUE(IsAllowlisted("http://siteb.tld/", "http://sitea.tld/"));
  EXPECT_TRUE(IsAllowlisted("http://sited.tld/", "http://sitea.tld/"));

  auto config = std::make_unique<reputation::SafetyTipsConfig>();
  AddAllowedPattern(*config, "sitee.tld/", {});
  SetSafetyTipsRemoteConfigProto(std::move(config));
  EXPECT_TRUE(IsAllowlisted("http://sitee.tld/", "http://sitea.tld/"));
  EXPECT_FALSE(IsAllowlisted("http://sited.tld/", "http://sitea.tld/"));

  SetSafetyTipsRemoteConfigProto(nullptr);
  EXPECT_FALSE(GetSafetyTipsRemoteConfigProto());
  EXPECT_FALSE(GetSafetyTipsRemoteAllowlistForTesting());
}

// Test helpers that modify and reinstall the installed config keep its
// allowlist.
TEST_F(FlatSafetyTipsAllowlistTest, TestHelpersKeepAllowlist) {
  const reputation::SafetyTipsConfig original = CreateConfig();
  InstallWithAllowlist(original);
  EXPECT_EQ(original.SerializeAsString(),
            GetOrCreateSafetyTipsConfig()->SerializeAsString());

  AddSafetyTipHeuristicLaunchConfigForTesting(
      reputation::HeuristicLaunchConfig::HEURISTIC_CHARACTER_SWAP_TOP_SITES,
      /*launch_percentage=*/100);
  EXPECT_EQ(2, GetSafetyTipsRemoteConfigProto()->launch_config_size());
  EXPECT_TRUE(IsAllowlisted("http://siteb.tld/", "http://sitea.tld/"));
}

}  // namespace lookalikes
