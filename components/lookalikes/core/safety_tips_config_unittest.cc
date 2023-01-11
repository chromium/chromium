// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/lookalikes/core/safety_tips_config.h"

#include "components/lookalikes/core/safety_tip_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace lookalikes {

// Build an allowlist with testable scoped allowlist entries.
void ConfigureAllowlistWithScopes() {
  auto config_proto = GetOrCreateSafetyTipsConfig();
  config_proto->clear_allowed_pattern();
  config_proto->clear_canonical_pattern();
  config_proto->clear_cohort();

  // Note that allowed_pattern *must* stay sorted.

  // error-canonical-index.tld has a cohort with an invalid allowed index.
  auto* pattern_bad_allowed = config_proto->add_allowed_pattern();
  pattern_bad_allowed->set_pattern("error-allowed-index.tld/");
  auto* cohort_bad_allowed = config_proto->add_cohort();
  cohort_bad_allowed->add_allowed_index(100);
  pattern_bad_allowed->add_cohort_index(0);  // cohort_bad_allowed

  // error-canonical-index.tld has a cohort with an invalid canonical index.
  auto* pattern_bad_canonical = config_proto->add_allowed_pattern();
  pattern_bad_canonical->set_pattern("error-canonical-index.tld/");
  auto* cohort_bad_canonical = config_proto->add_cohort();
  cohort_bad_canonical->add_canonical_index(100);
  pattern_bad_canonical->add_cohort_index(1);  // cohort_bad_canonical

  // error-cohort-index.tld has an invalid index.
  auto* pattern_bad_cohort = config_proto->add_allowed_pattern();
  pattern_bad_cohort->set_pattern("error-cohort-index.tld/");
  pattern_bad_cohort->add_cohort_index(100);

  // siteA.tld is only a canonical_pattern, so can't spoof anyone.
  config_proto->add_canonical_pattern()->set_pattern("sitea.tld/");

  // siteB.tld is only allowed to spoof siteA.tld and itself.
  auto* pattern_b = config_proto->add_allowed_pattern();
  pattern_b->set_pattern("siteb.tld/");
  auto* cohort_b = config_proto->add_cohort();
  cohort_b->add_allowed_index(3);    // siteB
  cohort_b->add_canonical_index(0);  // siteA
  pattern_b->add_cohort_index(2);    // cohort_b

  // siteC.tld is allowed to spoof siteB.tld and itself.
  auto* pattern_c = config_proto->add_allowed_pattern();
  pattern_c->set_pattern("sitec.tld/");
  auto* cohort_c = config_proto->add_cohort();
  cohort_c->add_allowed_index(3);  // siteB
  cohort_c->add_allowed_index(4);  // siteC
  pattern_c->add_cohort_index(3);  // cohort_c

  // siteD.tld is allowed to spoof anyone, so has no cohort.
  auto* pattern_d = config_proto->add_allowed_pattern();
  pattern_d->set_pattern("sited.tld/");

  // Implicitly, siteE.tld can't spoof anyone, since it isn't in the proto.

  SetSafetyTipsRemoteConfigProto(std::move(config_proto));
}

// Minimal test for an unscoped allowlist entry.
TEST(SafetyTipsConfigTest, TestBasicUrlAllowlist) {
  SetSafetyTipAllowlistPatterns({"example.com/"}, {}, {});
  auto* config = GetSafetyTipsRemoteConfigProto();

  // Basic unscoped entries are allowed to spoof any ("canonical") domain.
  EXPECT_TRUE(IsUrlAllowlistedBySafetyTipsComponent(
      config, GURL("http://example.com"), GURL("http://example.com")));
  EXPECT_TRUE(IsUrlAllowlistedBySafetyTipsComponent(
      config, GURL("http://example.com"), GURL("http://example.org")));

  EXPECT_FALSE(IsUrlAllowlistedBySafetyTipsComponent(
      config, GURL("http://example.org"), GURL("http://example.org")));
}

// Tests for a scoped allowlist (i.e. entries not permitted to spoof anything).
TEST(SafetyTipsConfigTest, TestScopedUrlAllowlist) {
  ConfigureAllowlistWithScopes();
  auto* config = GetSafetyTipsRemoteConfigProto();

  // Site A is only a canonical domain, so can't spoof anything.
  EXPECT_FALSE(IsUrlAllowlistedBySafetyTipsComponent(
      config, GURL("http://sitea.tld"), GURL("http://sitea.tld")));
  EXPECT_FALSE(IsUrlAllowlistedBySafetyTipsComponent(
      config, GURL("http://sitea.tld"), GURL("http://siteb.tld")));
  EXPECT_FALSE(IsUrlAllowlistedBySafetyTipsComponent(
      config, GURL("http://sitea.tld"), GURL("http://sitee.tld")));

  // Site B can spoof sites A & B, but not other stuff.
  EXPECT_TRUE(IsUrlAllowlistedBySafetyTipsComponent(
      config, GURL("http://siteb.tld"), GURL("http://sitea.tld")));
  EXPECT_TRUE(IsUrlAllowlistedBySafetyTipsComponent(
      config, GURL("http://siteb.tld"), GURL("http://siteb.tld")));
  EXPECT_FALSE(IsUrlAllowlistedBySafetyTipsComponent(
      config, GURL("http://siteb.tld"), GURL("http://sitec.tld")));
  EXPECT_FALSE(IsUrlAllowlistedBySafetyTipsComponent(
      config, GURL("http://siteb.tld"), GURL("http://sited.tld")));
  EXPECT_FALSE(IsUrlAllowlistedBySafetyTipsComponent(
      config, GURL("http://siteb.tld"), GURL("http://sitee.tld")));

  // Site C can spoof sites B and C, but not anything else.
  EXPECT_FALSE(IsUrlAllowlistedBySafetyTipsComponent(
      config, GURL("http://sitec.tld"), GURL("http://sitea.tld")));
  EXPECT_TRUE(IsUrlAllowlistedBySafetyTipsComponent(
      config, GURL("http://sitec.tld"), GURL("http://siteb.tld")));
  EXPECT_TRUE(IsUrlAllowlistedBySafetyTipsComponent(
      config, GURL("http://sitec.tld"), GURL("http://sitec.tld")));
  EXPECT_FALSE(IsUrlAllowlistedBySafetyTipsComponent(
      config, GURL("http://sitec.tld"), GURL("http://sited.tld")));
  EXPECT_FALSE(IsUrlAllowlistedBySafetyTipsComponent(
      config, GURL("http://sitec.tld"), GURL("http://sitee.tld")));

  // Site D has a wildcard, and can spoof anyone.
  EXPECT_TRUE(IsUrlAllowlistedBySafetyTipsComponent(
      config, GURL("http://sited.tld"), GURL("http://sitea.tld")));
  EXPECT_TRUE(IsUrlAllowlistedBySafetyTipsComponent(
      config, GURL("http://sited.tld"), GURL("http://siteb.tld")));
  EXPECT_TRUE(IsUrlAllowlistedBySafetyTipsComponent(
      config, GURL("http://sited.tld"), GURL("http://sitec.tld")));
  EXPECT_TRUE(IsUrlAllowlistedBySafetyTipsComponent(
      config, GURL("http://sited.tld"), GURL("http://sited.tld")));
  EXPECT_TRUE(IsUrlAllowlistedBySafetyTipsComponent(
      config, GURL("http://sited.tld"), GURL("http://sitee.tld")));

  // These sites all have invalid indices in their entries. Each should
  // invalidate the relevant part of their allowlist entry (i.e. act as if it's
  // not there), rather than crashing.
  EXPECT_FALSE(IsUrlAllowlistedBySafetyTipsComponent(
      config, GURL("http://error-cohort-index.tld"), GURL("http://sitea.tld")));
  EXPECT_FALSE(IsUrlAllowlistedBySafetyTipsComponent(
      config, GURL("http://error-canonical-index.tld"),
      GURL("http://sitea.tld")));
  EXPECT_FALSE(IsUrlAllowlistedBySafetyTipsComponent(
      config, GURL("http://error-allowed-index.tld"),
      GURL("http://sitea.tld")));
}

TEST(SafetyTipsConfigTest, TestTargetUrlAllowlist) {
  SetSafetyTipAllowlistPatterns({}, {"exa.*\\.com"}, {});
  auto* config = GetSafetyTipsRemoteConfigProto();
  EXPECT_TRUE(
      IsTargetHostAllowlistedBySafetyTipsComponent(config, "example.com"));
  EXPECT_FALSE(
      IsTargetHostAllowlistedBySafetyTipsComponent(config, "example.org"));
}

TEST(SafetyTipsConfigTest, TestCommonWords) {
  // IsCommonWordInConfigProto does a binary search of sorted common words.
  SetSafetyTipAllowlistPatterns({}, {}, {"common3", "common1", "common2"});
  auto* config = GetSafetyTipsRemoteConfigProto();
  EXPECT_TRUE(IsCommonWordInConfigProto(config, "common1"));
  EXPECT_TRUE(IsCommonWordInConfigProto(config, "common2"));
  EXPECT_TRUE(IsCommonWordInConfigProto(config, "common3"));
  EXPECT_FALSE(IsCommonWordInConfigProto(config, "uncommon"));
}

}  // namespace lookalikes
