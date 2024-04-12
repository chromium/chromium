// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tpcd/metadata/manager.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <tuple>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_enums.mojom-shared.h"
#include "components/content_settings/core/common/features.h"
#include "components/tpcd/metadata/metadata.pb.h"
#include "components/tpcd/metadata/parser.h"
#include "net/base/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tpcd::metadata {

class ManagerTest : public testing::Test,
                    public testing::WithParamInterface<
                        std::tuple</*kTpcdMetadataGrants*/ bool,
                                   /*kIndexedHostContentSettingsMap*/ bool>> {
 public:
  ManagerTest() = default;
  ~ManagerTest() override = default;

  bool IsTpcdMetadataGrantsEnabled() { return std::get<0>(GetParam()); }
  bool IsIndexedHostContentSettingsMapEnabled() {
    return std::get<1>(GetParam());
  }

  Parser* GetParser() {
    if (!parser_) {
      parser_ = std::make_unique<Parser>();
    }
    return parser_.get();
  }

  Manager* GetManager(GrantsSyncCallback callback = base::NullCallback()) {
    if (!manager_) {
      manager_ = std::make_unique<Manager>(GetParser(), callback);
    }
    return manager_.get();
  }

 protected:
  base::test::TaskEnvironment env_;

  void SetUp() override {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    if (IsTpcdMetadataGrantsEnabled()) {
      enabled_features.push_back(net::features::kTpcdMetadataGrants);
    } else {
      disabled_features.push_back(net::features::kTpcdMetadataGrants);
    }

    if (IsIndexedHostContentSettingsMapEnabled()) {
      enabled_features.push_back(
          content_settings::features::kIndexedHostContentSettingsMap);
    } else {
      disabled_features.push_back(
          content_settings::features::kIndexedHostContentSettingsMap);
    }

    scoped_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  // Guarantees proper tear down of dependencies.
  void TearDown() override {
    delete manager_.release();
    delete parser_.release();
  }

 private:
  base::test::ScopedFeatureList scoped_list_;
  std::unique_ptr<Parser> parser_;
  std::unique_ptr<Manager> manager_;
};

INSTANTIATE_TEST_SUITE_P(
    /* no label */,
    ManagerTest,
    testing::Combine(testing::Bool(), testing::Bool()));

TEST_P(ManagerTest, OnMetadataReady) {
  const std::string primary_pattern_spec = "https://www.der.com";
  const std::string secondary_pattern_spec = "https://www.foo.com";

  Metadata metadata;
  helpers::AddEntryToMetadata(metadata, primary_pattern_spec,
                              secondary_pattern_spec);
  EXPECT_EQ(metadata.metadata_entries_size(), 1);
  GetParser()->ParseMetadata(metadata.SerializeAsString());

  Manager* manager = GetManager();
  if (!IsTpcdMetadataGrantsEnabled()) {
    EXPECT_TRUE(manager->GetGrants().empty());
  } else {
    EXPECT_EQ(manager->GetGrants().size(), 1u);
    EXPECT_EQ(manager->GetGrants().front().primary_pattern.ToString(),
              primary_pattern_spec);
    EXPECT_EQ(manager->GetGrants().front().secondary_pattern.ToString(),
              secondary_pattern_spec);
    EXPECT_EQ(manager->GetGrants().front().metadata.tpcd_metadata_rule_source(),
              content_settings::mojom::TpcdMetadataRuleSource::SOURCE_TEST);
  }
}

TEST_P(ManagerTest, IsAllowed) {
  const std::string primary_pattern_spec = "https://www.der.com";
  const std::string secondary_pattern_spec = "https://www.foo.com";

  GURL first_party_url = GURL(secondary_pattern_spec);
  GURL third_party_url = GURL(primary_pattern_spec);
  GURL third_party_url_no_grants = GURL("https://www.bar.com");

  Metadata metadata;
  helpers::AddEntryToMetadata(metadata, primary_pattern_spec,
                              secondary_pattern_spec);
  EXPECT_EQ(metadata.metadata_entries_size(), 1);

  Manager* manager = GetManager();
  GetParser()->ParseMetadata(metadata.SerializeAsString());

  {
    content_settings::SettingInfo out_info;
    EXPECT_EQ(manager->IsAllowed(third_party_url, first_party_url, &out_info),
              IsTpcdMetadataGrantsEnabled());
    EXPECT_EQ(out_info.primary_pattern.ToString(),
              IsTpcdMetadataGrantsEnabled() ? primary_pattern_spec : "*");
    EXPECT_EQ(out_info.secondary_pattern.ToString(),
              IsTpcdMetadataGrantsEnabled() ? secondary_pattern_spec : "*");
    EXPECT_EQ(out_info.metadata.tpcd_metadata_rule_source(),
              IsTpcdMetadataGrantsEnabled()
                  ? content_settings::mojom::TpcdMetadataRuleSource::SOURCE_TEST
                  : content_settings::mojom::TpcdMetadataRuleSource::
                        SOURCE_UNSPECIFIED);
  }

  {
    content_settings::SettingInfo out_info;
    EXPECT_FALSE(manager->IsAllowed(third_party_url_no_grants, first_party_url,
                                    &out_info));
    EXPECT_EQ(out_info.primary_pattern.ToString(), "*");
    EXPECT_EQ(out_info.secondary_pattern.ToString(), "*");
    EXPECT_EQ(
        out_info.metadata.tpcd_metadata_rule_source(),
        content_settings::mojom::TpcdMetadataRuleSource::SOURCE_UNSPECIFIED);
  }
}

TEST_P(ManagerTest, FireSyncCallback) {
  base::RunLoop run_loop;
  const std::string primary_pattern_spec = "https://www.der.com";
  const std::string secondary_pattern_spec = "https://www.foo.com";

  auto dummy_callback = [&](const ContentSettingsForOneType& grants) {
    run_loop.Quit();
    if (!IsTpcdMetadataGrantsEnabled()) {
      EXPECT_TRUE(grants.empty());
    } else {
      EXPECT_EQ(grants.size(), 1u);
      EXPECT_EQ(grants.front().primary_pattern.ToString(),
                primary_pattern_spec);
      EXPECT_EQ(grants.front().secondary_pattern.ToString(),
                secondary_pattern_spec);
      EXPECT_EQ(grants.front().metadata.tpcd_metadata_rule_source(),
                content_settings::mojom::TpcdMetadataRuleSource::SOURCE_TEST);
    }
  };
  Manager* manager = GetManager(base::BindLambdaForTesting(dummy_callback));

  Metadata metadata;
  helpers::AddEntryToMetadata(metadata, primary_pattern_spec,
                              secondary_pattern_spec);
  EXPECT_EQ(metadata.metadata_entries_size(), 1);

  GetParser()->ParseMetadata(metadata.SerializeAsString());
  if (!IsTpcdMetadataGrantsEnabled()) {
    EXPECT_TRUE(manager->GetGrants().empty());
  } else {
    EXPECT_EQ(manager->GetGrants().size(), 1u);
    run_loop.Run();
  }
}

class DeterministicManager : public Manager {
 public:
  DeterministicManager(Parser* parser, GrantsSyncCallback callback)
      : Manager(parser, callback) {}
  ~DeterministicManager() override = default;

  // Returns a deterministic "random" value for testing.
  uint32_t GenerateRand() const override { return rand_; }

  void set_rand(uint32_t rand) { rand_ = rand; }

 private:
  uint32_t rand_ = 0;
};

class ManagerCohortsTest
    : public testing::Test,
      public testing::WithParamInterface<std::tuple<bool, int32_t>> {
 public:
  ManagerCohortsTest() = default;
  ~ManagerCohortsTest() override = default;

  Parser* GetParser() {
    if (!parser_) {
      parser_ = std::make_unique<Parser>();
    }
    return parser_.get();
  }

  Manager* GetManager(GrantsSyncCallback callback = base::NullCallback()) {
    if (!manager_) {
      manager_ = std::make_unique<Manager>(GetParser(), callback);
    }
    return manager_.get();
  }

  DeterministicManager* GetDeterministicManager(
      GrantsSyncCallback callback = base::NullCallback()) {
    if (!det_manager_) {
      det_manager_ =
          std::make_unique<DeterministicManager>(GetParser(), callback);
    }
    return det_manager_.get();
  }

  bool IsTpcdMetadataStagedRollbackEnabled() { return std::get<0>(GetParam()); }
  content_settings::mojom::TpcdMetadataRuleSource GetTpcdMetadataRuleSource() {
    return static_cast<content_settings::mojom::TpcdMetadataRuleSource>(
        std::get<1>(GetParam()));
  }

  std::string ToRuleSourceStr(
      const content_settings::mojom::TpcdMetadataRuleSource& rule_source) {
    switch (rule_source) {
      case content_settings::mojom::TpcdMetadataRuleSource::SOURCE_UNSPECIFIED:
        return Parser::kSourceUnspecified;
      case content_settings::mojom::TpcdMetadataRuleSource::SOURCE_TEST:
        return Parser::kSourceTest;
      case content_settings::mojom::TpcdMetadataRuleSource::SOURCE_1P_DT:
        return Parser::kSource1pDt;
      case content_settings::mojom::TpcdMetadataRuleSource::SOURCE_3P_DT:
        return Parser::kSource3pDt;
      case content_settings::mojom::TpcdMetadataRuleSource::SOURCE_DOGFOOD:
        return Parser::kSourceDogFood;
      case content_settings::mojom::TpcdMetadataRuleSource::
          SOURCE_CRITICAL_SECTOR:
        return Parser::kSourceCriticalSector;
      case content_settings::mojom::TpcdMetadataRuleSource::SOURCE_CUJ:
        return Parser::kSourceCuj;
      case content_settings::mojom::TpcdMetadataRuleSource::SOURCE_GOV_EDU_TLD:
        return Parser::kSourceGovEduTld;
    }
  }

 protected:
  void SetUp() override {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    enabled_features.push_back(net::features::kTpcdMetadataGrants);
    enabled_features.push_back(
        content_settings::features::kIndexedHostContentSettingsMap);

    if (IsTpcdMetadataStagedRollbackEnabled()) {
      enabled_features.push_back(net::features::kTpcdMetadataStagedRollback);
    } else {
      disabled_features.push_back(net::features::kTpcdMetadataStagedRollback);
    }

    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  // Guarantees proper tear down of dependencies.
  void TearDown() override {
    delete manager_.release();
    delete det_manager_.release();
    delete parser_.release();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<Parser> parser_;
  std::unique_ptr<Manager> manager_;
  std::unique_ptr<DeterministicManager> det_manager_;
};

INSTANTIATE_TEST_SUITE_P(
    /* no label */,
    ManagerCohortsTest,
    testing::Combine(
        testing::Bool(),
        testing::Range<int32_t>(
            static_cast<int32_t>(
                content_settings::mojom::TpcdMetadataRuleSource::kMinValue),
            static_cast<int32_t>(
                content_settings::mojom::TpcdMetadataRuleSource::kMaxValue),
            /*step=*/1)));

TEST_P(ManagerCohortsTest, DTRP_Eligibility) {
  const std::string primary_pattern_spec = "https://www.der.com";
  const std::string secondary_pattern_spec = "https://www.foo.com";

  Metadata metadata;
  helpers::AddEntryToMetadata(metadata, primary_pattern_spec,
                              secondary_pattern_spec,
                              ToRuleSourceStr(GetTpcdMetadataRuleSource()));
  EXPECT_EQ(metadata.metadata_entries_size(), 1);

  Manager* manager = GetManager();
  GetParser()->ParseMetadata(metadata.SerializeAsString());

  EXPECT_EQ(manager->GetGrants().size(), 1u);
  EXPECT_EQ(manager->GetGrants().front().primary_pattern.ToString(),
            primary_pattern_spec);
  EXPECT_EQ(manager->GetGrants().front().secondary_pattern.ToString(),
            secondary_pattern_spec);

  auto rule_source =
      manager->GetGrants().front().metadata.tpcd_metadata_rule_source();
  EXPECT_EQ(rule_source, GetTpcdMetadataRuleSource());

  switch (GetTpcdMetadataRuleSource()) {
    case content_settings::mojom::TpcdMetadataRuleSource::SOURCE_1P_DT:
    case content_settings::mojom::TpcdMetadataRuleSource::SOURCE_3P_DT:
      EXPECT_TRUE(Parser::IsDtrpEligible(rule_source));
      break;
    case content_settings::mojom::TpcdMetadataRuleSource::SOURCE_DOGFOOD:
    case content_settings::mojom::TpcdMetadataRuleSource::
        SOURCE_CRITICAL_SECTOR:
    case content_settings::mojom::TpcdMetadataRuleSource::SOURCE_CUJ:
    case content_settings::mojom::TpcdMetadataRuleSource::SOURCE_GOV_EDU_TLD:
    case content_settings::mojom::TpcdMetadataRuleSource::SOURCE_TEST:
    case content_settings::mojom::TpcdMetadataRuleSource::SOURCE_UNSPECIFIED:
      EXPECT_FALSE(Parser::IsDtrpEligible(rule_source));
      break;
  }
}

TEST_P(ManagerCohortsTest, DTRP_0Percent) {
  const uint32_t dtrp_being_tested = 0;

  const std::string primary_pattern_spec = "https://www.der.com";
  const std::string secondary_pattern_spec = "https://www.foo.com";

  Metadata metadata;
  helpers::AddEntryToMetadata(
      metadata, primary_pattern_spec, secondary_pattern_spec,
      ToRuleSourceStr(GetTpcdMetadataRuleSource()), dtrp_being_tested);
  EXPECT_EQ(metadata.metadata_entries_size(), 1);

  Manager* manager = GetManager();
  GetParser()->ParseMetadata(metadata.SerializeAsString());

  EXPECT_EQ(manager->GetGrants().size(), 1u);
  EXPECT_EQ(manager->GetGrants().front().primary_pattern.ToString(),
            primary_pattern_spec);
  EXPECT_EQ(manager->GetGrants().front().secondary_pattern.ToString(),
            secondary_pattern_spec);

  auto rule_source =
      manager->GetGrants().front().metadata.tpcd_metadata_rule_source();
  EXPECT_EQ(rule_source, GetTpcdMetadataRuleSource());

  auto picked_cohort =
      manager->GetGrants().front().metadata.tpcd_metadata_cohort();
  if (IsTpcdMetadataStagedRollbackEnabled() &&
      Parser::IsDtrpEligible(rule_source)) {
    EXPECT_EQ(
        picked_cohort,
        content_settings::mojom::TpcdMetadataCohort::GRACE_PERIOD_FORCED_ON);
  } else {
    EXPECT_EQ(picked_cohort,
              content_settings::mojom::TpcdMetadataCohort::DEFAULT);
  }
}

TEST_P(ManagerCohortsTest, DTRP_100Percent) {
  const uint32_t dtrp_being_tested = 100;

  const std::string primary_pattern_spec = "https://www.der.com";
  const std::string secondary_pattern_spec = "https://www.foo.com";

  Metadata metadata;
  helpers::AddEntryToMetadata(
      metadata, primary_pattern_spec, secondary_pattern_spec,
      ToRuleSourceStr(GetTpcdMetadataRuleSource()), dtrp_being_tested);
  EXPECT_EQ(metadata.metadata_entries_size(), 1);

  Manager* manager = GetManager();
  GetParser()->ParseMetadata(metadata.SerializeAsString());

  EXPECT_EQ(manager->GetGrants().size(), 1u);
  EXPECT_EQ(manager->GetGrants().front().primary_pattern.ToString(),
            primary_pattern_spec);
  EXPECT_EQ(manager->GetGrants().front().secondary_pattern.ToString(),
            secondary_pattern_spec);

  auto rule_source =
      manager->GetGrants().front().metadata.tpcd_metadata_rule_source();
  EXPECT_EQ(rule_source, GetTpcdMetadataRuleSource());

  auto picked_cohort =
      manager->GetGrants().front().metadata.tpcd_metadata_cohort();
  if (IsTpcdMetadataStagedRollbackEnabled() &&
      Parser::IsDtrpEligible(rule_source)) {
    EXPECT_EQ(
        picked_cohort,
        content_settings::mojom::TpcdMetadataCohort::GRACE_PERIOD_FORCED_OFF);
  } else {
    EXPECT_EQ(picked_cohort,
              content_settings::mojom::TpcdMetadataCohort::DEFAULT);
  }
}

TEST_P(ManagerCohortsTest, DTRP_GE_Rand) {
  // dtrp_being_tested is arbitrary here, selected between (0,100).
  const uint32_t dtrp_being_tested = 55;
  // rand_being_tested is set as-is here to test the boundary between behaviors.
  const uint32_t rand_being_tested = dtrp_being_tested;

  const std::string primary_pattern_spec = "https://www.der.com";
  const std::string secondary_pattern_spec = "https://www.foo.com";

  Metadata metadata;
  helpers::AddEntryToMetadata(
      metadata, primary_pattern_spec, secondary_pattern_spec,
      ToRuleSourceStr(GetTpcdMetadataRuleSource()), dtrp_being_tested);
  EXPECT_EQ(metadata.metadata_entries_size(), 1);

  DeterministicManager* manager = GetDeterministicManager();
  manager->set_rand(rand_being_tested);
  GetParser()->ParseMetadata(metadata.SerializeAsString());

  EXPECT_EQ(manager->GetGrants().size(), 1u);
  EXPECT_EQ(manager->GetGrants().front().primary_pattern.ToString(),
            primary_pattern_spec);
  EXPECT_EQ(manager->GetGrants().front().secondary_pattern.ToString(),
            secondary_pattern_spec);

  auto rule_source =
      manager->GetGrants().front().metadata.tpcd_metadata_rule_source();
  EXPECT_EQ(rule_source, GetTpcdMetadataRuleSource());

  auto picked_cohort =
      manager->GetGrants().front().metadata.tpcd_metadata_cohort();
  if (IsTpcdMetadataStagedRollbackEnabled() &&
      Parser::IsDtrpEligible(rule_source)) {
    EXPECT_EQ(
        picked_cohort,
        content_settings::mojom::TpcdMetadataCohort::GRACE_PERIOD_FORCED_OFF);
  } else {
    EXPECT_EQ(picked_cohort,
              content_settings::mojom::TpcdMetadataCohort::DEFAULT);
  }
}

TEST_P(ManagerCohortsTest, DTRP_LT_Rand) {
  // dtrp_being_tested is arbitrary here, selected between (0,100).
  const uint32_t dtrp_being_tested = 55;
  // rand_being_tested is set as-is here to test the boundary between behaviors.
  const uint32_t rand_being_tested = dtrp_being_tested + 1;

  const std::string primary_pattern_spec = "https://www.der.com";
  const std::string secondary_pattern_spec = "https://www.foo.com";

  Metadata metadata;
  helpers::AddEntryToMetadata(
      metadata, primary_pattern_spec, secondary_pattern_spec,
      ToRuleSourceStr(GetTpcdMetadataRuleSource()), dtrp_being_tested);
  EXPECT_EQ(metadata.metadata_entries_size(), 1);

  DeterministicManager* manager = GetDeterministicManager();
  manager->set_rand(rand_being_tested);
  GetParser()->ParseMetadata(metadata.SerializeAsString());

  EXPECT_EQ(manager->GetGrants().size(), 1u);
  EXPECT_EQ(manager->GetGrants().front().primary_pattern.ToString(),
            primary_pattern_spec);
  EXPECT_EQ(manager->GetGrants().front().secondary_pattern.ToString(),
            secondary_pattern_spec);

  auto rule_source =
      manager->GetGrants().front().metadata.tpcd_metadata_rule_source();
  EXPECT_EQ(rule_source, GetTpcdMetadataRuleSource());

  auto picked_cohort =
      manager->GetGrants().front().metadata.tpcd_metadata_cohort();
  if (IsTpcdMetadataStagedRollbackEnabled() &&
      Parser::IsDtrpEligible(rule_source)) {
    EXPECT_EQ(
        picked_cohort,
        content_settings::mojom::TpcdMetadataCohort::GRACE_PERIOD_FORCED_ON);
  } else {
    EXPECT_EQ(picked_cohort,
              content_settings::mojom::TpcdMetadataCohort::DEFAULT);
  }
}

}  // namespace tpcd::metadata
