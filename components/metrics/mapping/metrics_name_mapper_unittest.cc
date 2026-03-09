// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/mapping/metrics_name_mapper.h"

#include "base/base64.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
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

 private:
  base::test::TaskEnvironment task_environment_;
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

}  // namespace
}  // namespace metrics
