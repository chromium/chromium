// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/component_metrics_provider.h"

#include "base/hash/hash.h"
#include "base/strings/utf_string_conversions.h"
#include "base/version.h"
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace metrics {

using component_updater::ComponentInfo;

namespace {
class TestComponentMetricsProviderDelegate
    : public ComponentMetricsProviderDelegate {
 public:
  explicit TestComponentMetricsProviderDelegate(
      std::vector<ComponentInfo>& components)
      : components_(components) {}
  ~TestComponentMetricsProviderDelegate() override = default;

  std::vector<ComponentInfo> GetComponents() override { return components_; }

 private:
  std::vector<ComponentInfo> components_;
};
}  // namespace

class ComponentMetricsProviderTest : public testing::Test {
 public:
  ComponentMetricsProviderTest() = default;

  ComponentMetricsProviderTest(const ComponentMetricsProviderTest&) = delete;
  ComponentMetricsProviderTest& operator=(const ComponentMetricsProviderTest&) =
      delete;

  ~ComponentMetricsProviderTest() override = default;
};

TEST_F(ComponentMetricsProviderTest, ProvideComponentMetrics) {
  std::vector<ComponentInfo> components = {
      ComponentInfo(
          "hfnkpimlhhgieaddgfemjhofmfblmnib",
          "1.0846414bf2025bbc067b6fa5b61b16eda2269d8712b8fec0973b4c71fdc65ca0",
          u"name1", base::Version("1.2.3.4"), ""),
      ComponentInfo(
          "oimompecagnajdejgnnjijobebaeigek",
          "1.adc9207a4a88ee98bf9ddf0330f35818386f1adc006bc8eee94dc59d43c0f5d6",
          u"name2", base::Version("5.6.7.8"), "1:1bcZ:55@0.33,67@0.5"),
      ComponentInfo(
          "thiscomponentfilteredfromresults",
          "1.b5268dc93e08d68d0be26bd8fbbb15c7b7f805cc06b4abd9d49381bc178e78cf",
          u"name3", base::Version("9.9.9.9"), "")};

  ComponentMetricsProvider component_provider(
      std::make_unique<TestComponentMetricsProviderDelegate>(components));
  SystemProfileProto system_profile;
  component_provider.ProvideSystemProfileMetrics(&system_profile);

  EXPECT_EQ(2, system_profile.chrome_component_size());
  EXPECT_EQ(SystemProfileProto_ComponentId_CRL_SET,
            system_profile.chrome_component(0).component_id());
  EXPECT_EQ("1.2.3.4", system_profile.chrome_component(0).version());
  EXPECT_EQ(138821963u, system_profile.chrome_component(0).omaha_fingerprint());
  EXPECT_EQ(SystemProfileProto_ComponentId_WIDEVINE_CDM,
            system_profile.chrome_component(1).component_id());
  EXPECT_EQ("5.6.7.8", system_profile.chrome_component(1).version());
  EXPECT_EQ(2915639418u,
            system_profile.chrome_component(1).omaha_fingerprint());
  EXPECT_EQ(system_profile.chrome_component(0).cohort_hash(),
            base::PersistentHash(""));
  EXPECT_EQ(system_profile.chrome_component(1).cohort_hash(),
            base::PersistentHash("1:1bcZ"));
}

}  // namespace metrics
