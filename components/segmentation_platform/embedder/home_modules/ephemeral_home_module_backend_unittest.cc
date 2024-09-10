// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/ephemeral_home_module_backend.h"

#include "base/test/scoped_feature_list.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/prefs/testing_pref_service.h"
#include "components/segmentation_platform/embedder/default_model/default_model_test_base.h"
#include "components/segmentation_platform/embedder/home_modules/card_selection_info.h"
#include "components/segmentation_platform/embedder/home_modules/card_selection_signals.h"
#include "components/segmentation_platform/embedder/home_modules/home_modules_card_registry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform::home_modules {

namespace {

// Test card info, with 4 signals, and 3 card variations. Shows label2 when
// first signal is non-zero, otherwise does not show.
class TestCardInfo : public CardSelectionInfo {
 public:
  static constexpr char kCountUma[] = "count_uma";
  static constexpr char kLatestUma[] = "latest_uma";
  static constexpr char kSumUma[] = "sum_uma";
  static constexpr char kInputShopping[] = "input_shopping";

  static constexpr char kLabel1[] = "label1";
  static constexpr char kLabel2[] = "label2";
  static constexpr char kLabel3[] = "label3";

  TestCardInfo() : CardSelectionInfo("test") {}
  ~TestCardInfo() override = default;

  std::vector<std::string> OutputLabels() override {
    return {kLabel1, kLabel2, kLabel3};
  }

  std::map<SignalKey, FeatureQuery> GetInputs() override {
    std::map<SignalKey, FeatureQuery> result;

    DEFINE_UMA_FEATURE_COUNT(count, "Uma.Metric", 7);
    result.emplace(kCountUma, std::move(count));
    DEFINE_UMA_FEATURE_LATEST(latest_uma, "Uma.Metric");
    result.emplace(kLatestUma, std::move(latest_uma));
    DEFINE_UMA_FEATURE_SUM(sum_uma, "Uma.Metric", 7);
    result.emplace(kSumUma, std::move(sum_uma));
    DEFINE_INPUT_CONTEXT(input_shopping, "shopping_input_count");
    result.emplace(kInputShopping, std::move(input_shopping));
    return result;
  }

  ShowResult ComputeCardResult(
      const CardSelectionSignals& signals) const override {
    if (signals.GetSignal(kCountUma) > 0) {
      return ShowResult(EphemeralHomeModuleRank::kTop, kLabel2);
    }
    return ShowResult(EphemeralHomeModuleRank::kNotShown);
  }
};

constexpr float kNotShownResultValue =
    EphemeralHomeModuleRankToScore(EphemeralHomeModuleRank::kNotShown);

class EphemeralHomeModuleBackendTest : public DefaultModelTestBase {
 public:
  EphemeralHomeModuleBackendTest()
      : DefaultModelTestBase(
            std::make_unique<EphemeralHomeModuleBackend>(nullptr)) {
    feature_list_.InitWithFeatures({commerce::kPriceTrackingPromo}, {});
    HomeModulesCardRegistry::RegisterProfilePrefs(pref_service_.registry());
    registry_ = std::make_unique<HomeModulesCardRegistry>(&pref_service_);
    static_cast<EphemeralHomeModuleBackend*>(model_.get())
        ->set_home_modules_card_registry_for_testing(registry_.get());
  }
  ~EphemeralHomeModuleBackendTest() override = default;

 protected:
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<HomeModulesCardRegistry> registry_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(EphemeralHomeModuleBackendTest, InitAndFetchModel) {
  ExpectInitAndFetchModel();
}

TEST_F(EphemeralHomeModuleBackendTest, ExecuteModelWithInput) {
#if BUILDFLAG(IS_IOS)
  ExpectExecutionWithInput({0, 0, 0}, /*expected_error=*/false,
                           /*expected_result=*/
                           {kNotShownResultValue, kNotShownResultValue});
#else
  ExpectExecutionWithInput(/*inputs=*/{}, /*expected_error=*/false,
                           /*expected_result=*/{kNotShownResultValue});
#endif
}

// Test with adding a TestCardInfo to the registry.
class EphemeralHomeModuleBackendWithTestCard : public DefaultModelTestBase {
 public:
  EphemeralHomeModuleBackendWithTestCard()
      : DefaultModelTestBase(
            std::make_unique<EphemeralHomeModuleBackend>(nullptr)) {
    std::vector<std::unique_ptr<CardSelectionInfo>> cards;
    cards.emplace_back(std::make_unique<TestCardInfo>());
    registry_ = std::make_unique<HomeModulesCardRegistry>(&pref_service_,
                                                          std::move(cards));
    static_cast<EphemeralHomeModuleBackend*>(model_.get())
        ->set_home_modules_card_registry_for_testing(registry_.get());
  }
  ~EphemeralHomeModuleBackendWithTestCard() override = default;

 protected:
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<HomeModulesCardRegistry> registry_;
};

TEST_F(EphemeralHomeModuleBackendWithTestCard, InitAndFetchModel) {
  // Validate model config.
  ExpectInitAndFetchModel();
}

TEST_F(EphemeralHomeModuleBackendWithTestCard, ExecuteModelWithInput) {
  ExpectInitAndFetchModel();

  // Run with all signals 0, no card should be shown.
  ModelProvider::Request inputs =
      ModelProvider::Request(registry_->all_cards_input_size(), 0);
  ExpectExecutionWithInput(
      inputs,
      /*expected_error=*/false,
      /*expected_result=*/
      ModelProvider::Response(registry_->all_output_labels().size(),
                              kNotShownResultValue));
  ExpectClassifierResults(inputs, {});

  // Run with first signal non-zero, label2 should be shown.
  inputs = ModelProvider::Request(registry_->all_cards_input_size(), 0);
  inputs[0] = 1;
  ModelProvider::Response expected_output = ModelProvider::Response(
      registry_->all_output_labels().size(), kNotShownResultValue);
  expected_output[registry_->get_label_index(TestCardInfo::kLabel2)] =
      EphemeralHomeModuleRankToScore(EphemeralHomeModuleRank::kTop);
  ExpectExecutionWithInput(inputs,
                           /*expected_error=*/false, expected_output);
  ExpectClassifierResults(inputs, {TestCardInfo::kLabel2});
}

}  // namespace

}  // namespace segmentation_platform::home_modules
