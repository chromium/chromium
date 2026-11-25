// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/mapping/metrics_name_mapper.h"

#include <array>
#include <memory>
#include <string>
#include <string_view>

#include "base/base64.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "components/metrics/mapping/metrics_mapping_features.h"
#include "components/metrics/mapping/metrics_name_mapping.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
namespace metrics {
namespace {

class MetricsNameMapperTest : public testing::Test {
 public:
  MetricsNameMapperTest() = default;
  ~MetricsNameMapperTest() override = default;

 protected:
  void SetUp() override { MetricsNameMapper::ResetDefaultsForTesting(); }
  void TearDown() override { MetricsNameMapper::ResetDefaultsForTesting(); }

  void SetUpMapperWithConfig(const MetricsNameMappingConfiguration& config) {
    scoped_feature_list_.Reset();

    std::string config_string;
    config.SerializeToString(&config_string);
    SetUpMapperWithBase64Config(base::Base64Encode(config_string));
  }

  void SetUpMapperWithBase64Config(const std::string& base64_config) {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kWebiumMetricsMapping, {{"config", base64_config}});

    mapper_ = std::make_unique<MetricsNameMapper>(base64_config);
  }

  MetricsNameMapper* mapper() { return mapper_.get(); }
  void DestroyMapper() { mapper_.reset(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<MetricsNameMapper> mapper_;
};

// Verifies that without any rules, all metrics are dropped (return empty
// string).
TEST_F(MetricsNameMapperTest, DefaultDrop) {
  MetricsNameMappingConfiguration config;
  SetUpMapperWithConfig(config);

  EXPECT_THAT(mapper()->GetMetricsNameIfAllowed("AnyHistogram"),
              testing::IsEmpty());
}

// Verifies that a rule precisely matches a metric by name, applying the given
// configuration to rename it. Non-matching metrics are dropped.
TEST_F(MetricsNameMapperTest, ExactMatchAllowWithRename) {
  MetricsNameMappingConfiguration config;
  MetricsNameMapping* rule = config.add_rules();
  rule->set_metric_name("TargetHistogram");
  rule->set_new_metric_name("RenamedHistogram");

  SetUpMapperWithConfig(config);

  EXPECT_EQ(mapper()->GetMetricsNameIfAllowed("TargetHistogram"),
            "RenamedHistogram");
  EXPECT_THAT(mapper()->GetMetricsNameIfAllowed("OtherHistogram"),
              testing::IsEmpty());
}

// Verifies that a rule with an empty new_metric_name is treated as if it were
// not set, allowing the metric to pass through with its original name.
TEST_F(MetricsNameMapperTest, ExactMatchAllowWithEmptyRenameIgnored) {
  MetricsNameMappingConfiguration config;
  MetricsNameMapping* rule = config.add_rules();
  rule->set_metric_name("TargetHistogram");
  rule->set_new_metric_name("");

  SetUpMapperWithConfig(config);

  EXPECT_EQ(mapper()->GetMetricsNameIfAllowed("TargetHistogram"),
            "TargetHistogram");
}

// Verifies that a rule precisely matches a metric by name but doesn't rename
// it, allowing it to pass through with original name.
TEST_F(MetricsNameMapperTest, ExactMatchAllowWithoutRename) {
  MetricsNameMappingConfiguration config;
  MetricsNameMapping* rule = config.add_rules();
  rule->set_metric_name("TargetHistogram");

  SetUpMapperWithConfig(config);

  EXPECT_EQ(mapper()->GetMetricsNameIfAllowed("TargetHistogram"),
            "TargetHistogram");
}

// Verifies that the first rule takes precedence for a given metric name.
TEST_F(MetricsNameMapperTest, ExactMatchFirstRuleWins) {
  MetricsNameMappingConfiguration config;

  MetricsNameMapping* rule1 = config.add_rules();
  rule1->set_metric_name("TargetHistogram");
  rule1->set_new_metric_name("FirstRename");

  MetricsNameMapping* rule2 = config.add_rules();
  rule2->set_metric_name("TargetHistogram");
  rule2->set_new_metric_name("SecondRename");

  SetUpMapperWithConfig(config);

  EXPECT_EQ(mapper()->GetMetricsNameIfAllowed("TargetHistogram"),
            "FirstRename");
}

// Verifies that a malformed Base64 string for the config feature param fails
// gracefully and defaults to DROP.
TEST_F(MetricsNameMapperTest, InvalidBase64Config) {
  SetUpMapperWithBase64Config("invalid_base64_%$#");

  // Since configuration fell back to nothing, it is treated as empty -> drop
  EXPECT_THAT(mapper()->GetMetricsNameIfAllowed("AnyHistogram"),
              testing::IsEmpty());
}

// Verifies that providing a valid Base64 string containing invalid protobuf
// bytes fails gracefully and defaults to DROP.
TEST_F(MetricsNameMapperTest, InvalidProtoConfig) {
  SetUpMapperWithBase64Config(base::Base64Encode("invalid_proto_data"));
  EXPECT_THAT(mapper()->GetMetricsNameIfAllowed("AnyHistogram"),
              testing::IsEmpty());
}

// Built-in rules and prefixes for the tests below, registered the way an
// embedder registers them.
constexpr auto kDefaultRules = std::to_array<MetricsNameMapper::Rule>({
    {"Renamed.Metric", "WebUI.TopChrome.Renamed.Metric"},
    {"Kept.Default", ""},
});
constexpr auto kDefaultAllowedPrefixes =
    std::to_array<std::string_view>({"WebUI."});

struct DefaultPrecedenceCase {
  std::string_view test_name;
  std::string_view input;
  std::string_view expected;
};

class MetricsNameMapperTestWithDefaults
    : public MetricsNameMapperTest,
      public testing::WithParamInterface<DefaultPrecedenceCase> {
 protected:
  void SetUp() override {
    MetricsNameMapperTest::SetUp();
    MetricsNameMapper::SetDefaults(kDefaultRules, kDefaultAllowedPrefixes);

    MetricsNameMappingConfiguration config;
    MetricsNameMapping* rename_conflict = config.add_rules();
    rename_conflict->set_metric_name("Renamed.Metric");
    rename_conflict->set_new_metric_name("Server.ShouldLose");

    MetricsNameMapping* keep_conflict = config.add_rules();
    keep_conflict->set_metric_name("Kept.Default");
    keep_conflict->set_new_metric_name("Server.ShouldAlsoLose");

    MetricsNameMapping* served_rule = config.add_rules();
    served_rule->set_metric_name("Served.Metric");
    served_rule->set_new_metric_name("Server.Renamed");

    config.add_rules()->set_metric_name("Kept.Metric");

    MetricsNameMapping* prefix_override = config.add_rules();
    prefix_override->set_metric_name("WebUI.Served");
    prefix_override->set_new_metric_name("Server.WebUI");

    SetUpMapperWithConfig(config);
  }
};

TEST_P(MetricsNameMapperTestWithDefaults, EvaluatesInPrecedenceOrder) {
  const auto& param = GetParam();
  EXPECT_EQ(mapper()->GetMetricsNameIfAllowed(param.input), param.expected);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    MetricsNameMapperTestWithDefaults,
    testing::Values(
        DefaultPrecedenceCase{"DefaultRenameBeatsServedRule", "Renamed.Metric",
                              "WebUI.TopChrome.Renamed.Metric"},
        DefaultPrecedenceCase{"DefaultKeepBeatsServedRule", "Kept.Default",
                              "Kept.Default"},
        DefaultPrecedenceCase{"ServedRuleRenames", "Served.Metric",
                              "Server.Renamed"},
        DefaultPrecedenceCase{"ServedRuleKeepsWithoutNewName", "Kept.Metric",
                              "Kept.Metric"},
        DefaultPrecedenceCase{"ServedRuleBeatsAllowedPrefix", "WebUI.Served",
                              "Server.WebUI"},
        DefaultPrecedenceCase{"AllowedPrefixKeeps", "WebUI.Other",
                              "WebUI.Other"},
        DefaultPrecedenceCase{"PrefixRequiresDot", "WebUIOther", ""},
        DefaultPrecedenceCase{"UnlistedDropped", "Unlisted.Metric", ""}),
    [](const testing::TestParamInfo<DefaultPrecedenceCase>& info) {
      return std::string(info.param.test_name);
    });

// Verifies that the defaults still apply when the served config is empty or
// cannot be parsed.
TEST_F(MetricsNameMapperTest, DefaultsApplyWithEmptyOrInvalidConfig) {
  MetricsNameMapper::SetDefaults(kDefaultRules, kDefaultAllowedPrefixes);

  for (const char* config : {"", "invalid_base64_%$#"}) {
    SetUpMapperWithBase64Config(config);
    EXPECT_EQ(mapper()->GetMetricsNameIfAllowed("Renamed.Metric"),
              "WebUI.TopChrome.Renamed.Metric");
    EXPECT_EQ(mapper()->GetMetricsNameIfAllowed("WebUI.Other"), "WebUI.Other");
    EXPECT_THAT(mapper()->GetMetricsNameIfAllowed("Unlisted.Metric"),
                testing::IsEmpty());
  }
}

// Verifies that the defaults turn off with `WebiumMetricsMapping`, because
// `CreateInstance()` then returns no mapper.
TEST_F(MetricsNameMapperTest, CreateInstanceAppliesDefaultsOnlyWhenEnabled) {
  MetricsNameMapper::SetDefaults(kDefaultRules, kDefaultAllowedPrefixes);

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kWebiumMetricsMapping);
  EXPECT_FALSE(MetricsNameMapper::CreateInstance());

  feature_list.Reset();
  feature_list.InitAndEnableFeature(features::kWebiumMetricsMapping);
  std::unique_ptr<MetricsNameMapper> instance =
      MetricsNameMapper::CreateInstance();
  ASSERT_TRUE(instance);
  EXPECT_EQ(instance->GetMetricsNameIfAllowed("Renamed.Metric"),
            "WebUI.TopChrome.Renamed.Metric");
  EXPECT_EQ(instance->GetMetricsNameIfAllowed("WebUI.Other"), "WebUI.Other");
  EXPECT_THAT(instance->GetMetricsNameIfAllowed("Unlisted.Metric"),
              testing::IsEmpty());
}

// Verifies that `SetDefaults()` rejects an empty metric name or prefix. An
// empty prefix would keep every metric.
TEST_F(MetricsNameMapperTest, SetDefaultsRejectsEmptyNames) {
  constexpr auto kEmptyName =
      std::to_array<MetricsNameMapper::Rule>({{"", "New.Name"}});
  constexpr auto kEmptyPrefix = std::to_array<std::string_view>({""});
  EXPECT_CHECK_DEATH(MetricsNameMapper::SetDefaults(kEmptyName, {}));
  EXPECT_CHECK_DEATH(MetricsNameMapper::SetDefaults({}, kEmptyPrefix));
}

// Verifies that `SetDefaults()` runs only once, so that it cannot free a name
// that an earlier lookup returned.
TEST_F(MetricsNameMapperTest, SetDefaultsRunsOnlyOnce) {
  MetricsNameMapper::SetDefaults(kDefaultRules, kDefaultAllowedPrefixes);
  EXPECT_CHECK_DEATH(
      MetricsNameMapper::SetDefaults(kDefaultRules, kDefaultAllowedPrefixes));
}

// Verifies that `SetDefaults()` cannot run after a mapper has been constructed.
TEST_F(MetricsNameMapperTest, SetDefaultsAfterMapperConstructionDies) {
  SetUpMapperWithBase64Config("");
  EXPECT_CHECK_DEATH(
      MetricsNameMapper::SetDefaults(kDefaultRules, kDefaultAllowedPrefixes));
}

// Verifies that a name returned for a built-in rule stays valid after the
// mapper that returned it is destroyed, because it points into the storage
// that `SetDefaults()` fills.
TEST_F(MetricsNameMapperTest, DefaultRenameOutlivesMapper) {
  MetricsNameMapper::SetDefaults(kDefaultRules, kDefaultAllowedPrefixes);
  SetUpMapperWithBase64Config("");
  std::string_view renamed =
      mapper()->GetMetricsNameIfAllowed("Renamed.Metric");

  DestroyMapper();
  EXPECT_EQ(renamed, "WebUI.TopChrome.Renamed.Metric");
}

}  // namespace
}  // namespace metrics
