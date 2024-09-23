// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tpcd/metadata/browser/manager.h"

#include <cstdint>
#include <memory>
#include <string>

#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_enums.mojom-shared.h"
#include "components/prefs/testing_pref_service.h"
#include "components/tpcd/metadata/browser/parser.h"
#include "components/tpcd/metadata/browser/prefs.h"
#include "components/tpcd/metadata/browser/test_support.h"
#include "components/tpcd/metadata/common/proto/metadata.pb.h"
#include "net/base/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tpcd::metadata {

namespace {

class TestTpcdManagerDelegate : public Manager::Delegate {
 public:
  TestTpcdManagerDelegate() {
    RegisterLocalStatePrefs(local_state_.registry());
  }

  void SetTpcdMetadataGrants(const ContentSettingsForOneType& grants) override {
    if (grants_callback_) {
      grants_callback_.Run(grants);
    }
  }

  PrefService& GetLocalState() override { return local_state_; }

  void set_grants_callback(
      base::RepeatingCallback<void(const ContentSettingsForOneType&)>
          grants_callback) {
    grants_callback_ = std::move(grants_callback);
  }

 private:
  base::RepeatingCallback<void(const ContentSettingsForOneType&)>
      grants_callback_;
  TestingPrefServiceSimple local_state_;
};

}  // namespace

class ManagerTest : public testing::Test,
                    public testing::WithParamInterface<
                        /*kTpcdMetadataGrants*/ bool> {
 public:
  ManagerTest() = default;
  ~ManagerTest() override = default;

  bool IsTpcdMetadataGrantsEnabled() const { return GetParam(); }

  Parser* GetParser() {
    if (!parser_) {
      parser_ = std::make_unique<Parser>();
    }
    return parser_.get();
  }

  Manager* GetManager() {
    if (!manager_) {
      manager_ = std::make_unique<Manager>(GetParser(), test_delegate_);
    }
    return manager_.get();
  }

 protected:
  void SetUp() override {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    if (IsTpcdMetadataGrantsEnabled()) {
      enabled_features.push_back(net::features::kTpcdMetadataGrants);
    } else {
      disabled_features.push_back(net::features::kTpcdMetadataGrants);
    }

    scoped_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  // Guarantees proper tear down of dependencies.
  void TearDown() override {
    delete manager_.release();
    delete parser_.release();
  }

  base::test::TaskEnvironment env_;
  base::test::ScopedFeatureList scoped_list_;
  std::unique_ptr<Parser> parser_;
  TestTpcdManagerDelegate test_delegate_;
  std::unique_ptr<Manager> manager_;
};

INSTANTIATE_TEST_SUITE_P(
    /* no label */,
    ManagerTest,
    testing::Bool());

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
    auto grant = manager->GetGrants().front();
    EXPECT_EQ(grant.primary_pattern.ToString(), primary_pattern_spec);
    EXPECT_EQ(grant.secondary_pattern.ToString(), secondary_pattern_spec);
    EXPECT_EQ(grant.metadata.tpcd_metadata_rule_source(),
              content_settings::mojom::TpcdMetadataRuleSource::SOURCE_TEST);
    EXPECT_EQ(grant.metadata.tpcd_metadata_elected_dtrp(), 0u);
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
  Manager* manager = GetManager();
  test_delegate_.set_grants_callback(
      base::BindLambdaForTesting(dummy_callback));

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

class ManagerCohortsTest : public testing::Test,
                           public testing::WithParamInterface<
                               /*IsTpcdMetadataStagedRollbackEnabled:*/ bool> {
 public:
  ManagerCohortsTest() = default;
  ~ManagerCohortsTest() override = default;

  Parser* GetParser() {
    if (!parser_) {
      parser_ = std::make_unique<Parser>();
    }
    return parser_.get();
  }

  Manager* GetManager() {
    if (!manager_) {
      manager_ = std::make_unique<Manager>(GetParser(), test_delegate_);
    }
    return manager_.get();
  }

  bool IsTpcdMetadataStagedRollbackEnabled() { return GetParam(); }

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

    if (IsTpcdMetadataStagedRollbackEnabled()) {
      enabled_features.push_back(net::features::kTpcdMetadataStageControl);
    } else {
      disabled_features.push_back(net::features::kTpcdMetadataStageControl);
    }

    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  // Guarantees proper tear down of dependencies.
  void TearDown() override {
    delete manager_.release();
    delete parser_.release();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<Parser> parser_;
  TestTpcdManagerDelegate test_delegate_;
  std::unique_ptr<Manager> manager_;
};

INSTANTIATE_TEST_SUITE_P(
    /* no label */,
    ManagerCohortsTest,
    testing::Bool());

TEST_P(ManagerCohortsTest, DTRP_0Percent) {
  const uint32_t dtrp_being_tested = 0;

  const std::string primary_pattern_spec = "https://www.der.com";
  const std::string secondary_pattern_spec = "https://www.foo.com";

  Metadata metadata;
  helpers::AddEntryToMetadata(metadata, primary_pattern_spec,
                              secondary_pattern_spec, Parser::kSourceTest,
                              dtrp_being_tested);
  EXPECT_EQ(metadata.metadata_entries_size(), 1);

  Manager* manager = GetManager();
  GetParser()->ParseMetadata(metadata.SerializeAsString());

  EXPECT_EQ(manager->GetGrants().size(), 1u);
  auto grant = manager->GetGrants().front();
  EXPECT_EQ(grant.primary_pattern.ToString(), primary_pattern_spec);
  EXPECT_EQ(grant.secondary_pattern.ToString(), secondary_pattern_spec);

  auto picked_cohort =
      manager->GetGrants().front().metadata.tpcd_metadata_cohort();
  if (IsTpcdMetadataStagedRollbackEnabled() &&
      metadata.metadata_entries().begin()->has_dtrp()) {
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
  helpers::AddEntryToMetadata(metadata, primary_pattern_spec,
                              secondary_pattern_spec, Parser::kSourceTest,
                              dtrp_being_tested);
  EXPECT_EQ(metadata.metadata_entries_size(), 1);

  Manager* manager = GetManager();
  GetParser()->ParseMetadata(metadata.SerializeAsString());

  EXPECT_EQ(manager->GetGrants().size(), 1u);
  auto grant = manager->GetGrants().front();
  EXPECT_EQ(grant.primary_pattern.ToString(), primary_pattern_spec);
  EXPECT_EQ(grant.secondary_pattern.ToString(), secondary_pattern_spec);

  auto picked_cohort =
      manager->GetGrants().front().metadata.tpcd_metadata_cohort();
  if (IsTpcdMetadataStagedRollbackEnabled() &&
      metadata.metadata_entries().begin()->has_dtrp()) {
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
  helpers::AddEntryToMetadata(metadata, primary_pattern_spec,
                              secondary_pattern_spec, Parser::kSourceTest,
                              dtrp_being_tested);
  EXPECT_EQ(metadata.metadata_entries_size(), 1);

  Manager* manager = GetManager();
  manager->SetRandGeneratorForTesting(
      new DeterministicGenerator(rand_being_tested));
  GetParser()->ParseMetadata(metadata.SerializeAsString());

  EXPECT_EQ(manager->GetGrants().size(), 1u);
  auto grant = manager->GetGrants().front();
  EXPECT_EQ(grant.primary_pattern.ToString(), primary_pattern_spec);
  EXPECT_EQ(grant.secondary_pattern.ToString(), secondary_pattern_spec);

  auto picked_cohort =
      manager->GetGrants().front().metadata.tpcd_metadata_cohort();
  if (IsTpcdMetadataStagedRollbackEnabled() &&
      metadata.metadata_entries().begin()->has_dtrp()) {
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
  helpers::AddEntryToMetadata(metadata, primary_pattern_spec,
                              secondary_pattern_spec, Parser::kSourceTest,
                              dtrp_being_tested);
  EXPECT_EQ(metadata.metadata_entries_size(), 1);

  Manager* manager = GetManager();
  manager->SetRandGeneratorForTesting(
      new DeterministicGenerator(rand_being_tested));
  GetParser()->ParseMetadata(metadata.SerializeAsString());

  EXPECT_EQ(manager->GetGrants().size(), 1u);
  auto grant = manager->GetGrants().front();
  EXPECT_EQ(grant.primary_pattern.ToString(), primary_pattern_spec);
  EXPECT_EQ(grant.secondary_pattern.ToString(), secondary_pattern_spec);

  auto picked_cohort =
      manager->GetGrants().front().metadata.tpcd_metadata_cohort();
  if (IsTpcdMetadataStagedRollbackEnabled() &&
      metadata.metadata_entries().begin()->has_dtrp()) {
    EXPECT_EQ(
        picked_cohort,
        content_settings::mojom::TpcdMetadataCohort::GRACE_PERIOD_FORCED_ON);
  } else {
    EXPECT_EQ(picked_cohort,
              content_settings::mojom::TpcdMetadataCohort::DEFAULT);
  }
}

TEST_P(ManagerCohortsTest, MetadataCohortDistributionUma) {
  Metadata metadata;
  for (const auto& source : testing::Range<int32_t>(
           static_cast<int32_t>(TpcdMetadataRuleSource::kMinValue),
           static_cast<int32_t>(TpcdMetadataRuleSource::kMaxValue) + 1,
           /*step=*/1)) {
    helpers::AddEntryToMetadata(
        metadata, "*",
        base::StrCat({"[*.]foo-", base::NumberToString(source), ".com"}),
        ToRuleSourceStr(static_cast<TpcdMetadataRuleSource>(source)),
        /*dtrp=*/0);
  }
  helpers::AddEntryToMetadata(metadata, "*", "[*.]foo.com");

  base::HistogramTester histogram_tester;
  GetParser()->ParseMetadata(metadata.SerializeAsString());
  EXPECT_EQ(GetManager()->GetGrants().size(), 9u);

  if (IsTpcdMetadataStagedRollbackEnabled()) {
    histogram_tester.ExpectUniqueSample(
        helpers::kMetadataCohortDistributionHistogram,
        content_settings::mojom::TpcdMetadataCohort::GRACE_PERIOD_FORCED_ON, 1);
  } else {
    histogram_tester.ExpectTotalCount(
        helpers::kMetadataCohortDistributionHistogram, 0);
  }
}

class ManagerPrefsTest : public testing::Test {
 public:
  ManagerPrefsTest() {
    det_generator_ = new DeterministicGenerator();
    GetManager()->SetRandGeneratorForTesting(det_generator_);
  }
  ~ManagerPrefsTest() override = default;

  Parser* GetParser() {
    if (!parser_) {
      parser_ = std::make_unique<Parser>();
    }
    return parser_.get();
  }

  Manager* GetManager() {
    if (!manager_) {
      manager_ = std::make_unique<Manager>(GetParser(), test_delegate_);
    }
    return manager_.get();
  }

  DeterministicGenerator* GetDetGenerator() { return det_generator_; }

  PrefService* GetLocalState() { return &test_delegate_.GetLocalState(); }

 protected:
  base::test::TaskEnvironment env_;

  void SetUp() override {
    scoped_list_.InitWithFeatures({net::features::kTpcdMetadataGrants,
                                   net::features::kTpcdMetadataStageControl},
                                  {});
  }

  // Guarantees proper tear down of dependencies.
  void TearDown() override {
    det_generator_ = nullptr;
    delete manager_.release();
    delete parser_.release();
  }

 private:
  base::test::ScopedFeatureList scoped_list_;
  raw_ptr<DeterministicGenerator> det_generator_;
  std::unique_ptr<Parser> parser_;
  TestTpcdManagerDelegate test_delegate_;
  std::unique_ptr<Manager> manager_;
};

TEST_F(ManagerPrefsTest, PersistedCohorts) {
  const std::string primary_pattern_spec = "https://www.der.com";
  const std::string secondary_pattern_spec = "https://www.foo.com";

  // First encounter of entry x. Expected to set a new state for entry x in the
  // prefs store.
  {
    // dtrp is arbitrary here, selected between (0,100).
    const uint32_t dtrp = 10;
    // rand is set as-is here to guarantee GRACE_PERIOD_FORCED_ON.
    uint32_t rand = dtrp + 1;
    Metadata metadata;
    helpers::AddEntryToMetadata(metadata, primary_pattern_spec,
                                secondary_pattern_spec, Parser::kSource1pDt,
                                dtrp);
    EXPECT_EQ(metadata.metadata_entries_size(), 1);

    Manager* manager = GetManager();
    GetDetGenerator()->set_rand(rand);
    GetParser()->ParseMetadata(metadata.SerializeAsString());

    EXPECT_EQ(manager->GetGrants().size(), 1u);
    auto grant = manager->GetGrants().front();
    EXPECT_EQ(grant.metadata.tpcd_metadata_elected_dtrp(), dtrp);
    auto picked_cohort = grant.metadata.tpcd_metadata_cohort();
    EXPECT_EQ(
        picked_cohort,
        content_settings::mojom::TpcdMetadataCohort::GRACE_PERIOD_FORCED_ON);

    const auto& dict = GetLocalState()->GetDict(prefs::kCohorts);
    EXPECT_EQ(dict.size(), 1u);
    EXPECT_EQ(dict.cbegin()->first,
              helpers::GenerateKeyHash(metadata.metadata_entries(0)));
    EXPECT_EQ(dict.cbegin()->second,
              static_cast<int>(content_settings::mojom::TpcdMetadataCohort::
                                   GRACE_PERIOD_FORCED_ON));
  }

  // Second encounter of the same entry x. Cohort will be sourced from the
  // locale state prefs.
  {
    // dtrp is arbitrary here, selected between (0,100).
    const uint32_t dtrp = 10;
    // rand is set as-is here to guarantee GRACE_PERIOD_FORCED_OFF.
    uint32_t rand = dtrp;
    Metadata metadata;
    helpers::AddEntryToMetadata(metadata, primary_pattern_spec,
                                secondary_pattern_spec, Parser::kSource1pDt,
                                dtrp);
    EXPECT_EQ(metadata.metadata_entries_size(), 1);

    Manager* manager = GetManager();
    GetDetGenerator()->set_rand(rand);
    GetParser()->ParseMetadata(metadata.SerializeAsString());

    EXPECT_EQ(manager->GetGrants().size(), 1u);
    auto grant = manager->GetGrants().front();
    EXPECT_EQ(grant.metadata.tpcd_metadata_elected_dtrp(), dtrp);
    auto stored_cohort = grant.metadata.tpcd_metadata_cohort();
    EXPECT_EQ(
        stored_cohort,
        content_settings::mojom::TpcdMetadataCohort::GRACE_PERIOD_FORCED_ON);

    const auto& dict = GetLocalState()->GetDict(prefs::kCohorts);
    EXPECT_EQ(dict.size(), 1u);
    EXPECT_EQ(dict.cbegin()->first,
              helpers::GenerateKeyHash(metadata.metadata_entries(0)));
    EXPECT_EQ(dict.cbegin()->second,
              static_cast<int>(content_settings::mojom::TpcdMetadataCohort::
                                   GRACE_PERIOD_FORCED_ON));
  }

  // First encounter of a modified version of entry x, say y. Expected to set a
  // new state for entry y in the prefs store and erase the that of entry x.
  {
    // dtrp is arbitrary here, selected between (0,100).
    const uint32_t dtrp = 50;
    // rand is set as-is here to guarantee GRACE_PERIOD_FORCED_ON.
    uint32_t rand = dtrp + 1;
    Metadata metadata;
    helpers::AddEntryToMetadata(metadata, primary_pattern_spec,
                                secondary_pattern_spec, Parser::kSource1pDt,
                                dtrp);
    EXPECT_EQ(metadata.metadata_entries_size(), 1);

    Manager* manager = GetManager();
    GetDetGenerator()->set_rand(rand);
    GetParser()->ParseMetadata(metadata.SerializeAsString());

    EXPECT_EQ(manager->GetGrants().size(), 1u);
    auto grant = manager->GetGrants().front();
    EXPECT_EQ(grant.metadata.tpcd_metadata_elected_dtrp(), dtrp);
    auto picked_cohort = grant.metadata.tpcd_metadata_cohort();
    EXPECT_EQ(
        picked_cohort,
        content_settings::mojom::TpcdMetadataCohort::GRACE_PERIOD_FORCED_ON);

    const auto& dict = GetLocalState()->GetDict(prefs::kCohorts);
    EXPECT_EQ(dict.size(), 1u);
    EXPECT_EQ(dict.cbegin()->first,
              helpers::GenerateKeyHash(metadata.metadata_entries(0)));
    EXPECT_EQ(dict.cbegin()->second,
              static_cast<int>(content_settings::mojom::TpcdMetadataCohort::
                                   GRACE_PERIOD_FORCED_ON));
  }

  // Third encounter of the same entry x. Cohort will be (randomly) re-picked
  // and stored in the locale state prefs.
  {
    // dtrp is arbitrary here, selected between (0,100).
    const uint32_t dtrp = 10;
    // rand is set as-is here to guarantee GRACE_PERIOD_FORCED_OFF.
    uint32_t rand = dtrp;
    Metadata metadata;
    helpers::AddEntryToMetadata(metadata, primary_pattern_spec,
                                secondary_pattern_spec, Parser::kSource1pDt,
                                dtrp);
    EXPECT_EQ(metadata.metadata_entries_size(), 1);

    Manager* manager = GetManager();
    GetDetGenerator()->set_rand(rand);
    GetParser()->ParseMetadata(metadata.SerializeAsString());

    EXPECT_EQ(manager->GetGrants().size(), 1u);
    auto grant = manager->GetGrants().front();
    EXPECT_EQ(grant.metadata.tpcd_metadata_elected_dtrp(), dtrp);
    auto picked_cohort = grant.metadata.tpcd_metadata_cohort();
    EXPECT_EQ(
        picked_cohort,
        content_settings::mojom::TpcdMetadataCohort::GRACE_PERIOD_FORCED_OFF);

    const auto& dict = GetLocalState()->GetDict(prefs::kCohorts);
    EXPECT_EQ(dict.size(), 1u);
    EXPECT_EQ(dict.cbegin()->first,
              helpers::GenerateKeyHash(metadata.metadata_entries(0)));
    EXPECT_EQ(dict.cbegin()->second,
              static_cast<int>(content_settings::mojom::TpcdMetadataCohort::
                                   GRACE_PERIOD_FORCED_OFF));
  }
}

}  // namespace tpcd::metadata
