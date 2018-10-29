// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/content/previews_optimization_guide.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "components/optimization_guide/optimization_guide_service.h"
#include "components/optimization_guide/optimization_guide_service_observer.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/previews/content/previews_user_data.h"
#include "components/previews/core/bloom_filter.h"
#include "components/previews/core/previews_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace previews {

namespace {
// A fake default page_id for testing.
const uint64_t kDefaultPageId = 123456;
}  // namespace

class TestOptimizationGuideService
    : public optimization_guide::OptimizationGuideService {
 public:
  explicit TestOptimizationGuideService(
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner)
      : OptimizationGuideService(io_task_runner),
        remove_observer_called_(false) {}

  void RemoveObserver(
      optimization_guide::OptimizationGuideServiceObserver* observer) override {
    remove_observer_called_ = true;
  }

  bool RemoveObserverCalled() { return remove_observer_called_; }

 private:
  bool remove_observer_called_;
};

class PreviewsOptimizationGuideTest : public testing::Test {
 public:
  PreviewsOptimizationGuideTest() {}

  ~PreviewsOptimizationGuideTest() override {}

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    optimization_guide_service_ =
        std::make_unique<TestOptimizationGuideService>(
            scoped_task_environment_.GetMainThreadTaskRunner());
    guide_ = std::make_unique<PreviewsOptimizationGuide>(
        optimization_guide_service_.get(),
        scoped_task_environment_.GetMainThreadTaskRunner());
  }

  // Delete |guide_| if it hasn't been deleted.
  void TearDown() override { ResetGuide(); }

  PreviewsOptimizationGuide* guide() { return guide_.get(); }

  TestOptimizationGuideService* optimization_guide_service() {
    return optimization_guide_service_.get();
  }

  void ProcessHints(const optimization_guide::proto::Configuration& config,
                    std::string version) {
    optimization_guide::ComponentInfo info(
        base::Version(version),
        temp_dir().Append(FILE_PATH_LITERAL("somefile.pb")));
    guide_->OnHintsProcessed(config, info);
  }

  void MaybeLoadOptimizationHintsCallback(
      const GURL& document_gurl,
      const std::vector<std::string>& resource_patterns) {
    loaded_hints_document_gurl_ = document_gurl;
    loaded_hints_resource_patterns_ = resource_patterns;
  }

  void ResetGuide() {
    guide_.reset();
    RunUntilIdle();
  }

  base::FilePath temp_dir() const { return temp_dir_.GetPath(); }

  const GURL& loaded_hints_document_gurl() const {
    return loaded_hints_document_gurl_;
  }
  const std::vector<std::string>& loaded_hints_resource_patterns() const {
    return loaded_hints_resource_patterns_;
  }

 protected:
  void RunUntilIdle() {
    scoped_task_environment_.RunUntilIdle();
    base::RunLoop().RunUntilIdle();
  }

  void DoExperimentFlagTest(base::Optional<std::string> experiment_name,
                            bool expect_enabled);

  // This is a helper function for initializing fixed number of ResourceLoading
  // hints.
  void InitializeFixedCountResourceLoadingHints();

  // This is a helper function for initializing multiple ResourceLoading hints.
  // The generated hint proto contains hints for |key_count| keys.
  // |page_patterns_per_key| page patterns are specified per key.
  // For each page pattern, 2 resource loading hints are specified in the proto.
  void InitializeMultipleResourceLoadingHints(size_t key_count,
                                              size_t page_patterns_per_key);

  // This is a helper function for initializing with a LITE_PAGE_REDIRECT
  // server blacklist.
  void InitializeWithLitePageRedirectBlacklist();

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  base::ScopedTempDir temp_dir_;

  std::unique_ptr<PreviewsOptimizationGuide> guide_;
  std::unique_ptr<TestOptimizationGuideService> optimization_guide_service_;

  GURL loaded_hints_document_gurl_;
  std::vector<std::string> loaded_hints_resource_patterns_;

  DISALLOW_COPY_AND_ASSIGN(PreviewsOptimizationGuideTest);
};

void PreviewsOptimizationGuideTest::InitializeFixedCountResourceLoadingHints() {
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint1 = config.add_hints();
  hint1->set_key("somedomain.org");
  hint1->set_key_representation(optimization_guide::proto::HOST_SUFFIX);

  // Page hint for "/news/"
  optimization_guide::proto::PageHint* page_hint1 = hint1->add_page_hints();
  page_hint1->set_page_pattern("/news/");
  optimization_guide::proto::Optimization* optimization1 =
      page_hint1->add_whitelisted_optimizations();
  optimization1->set_optimization_type(
      optimization_guide::proto::RESOURCE_LOADING);
  optimization_guide::proto::ResourceLoadingHint* resource_loading_hint1 =
      optimization1->add_resource_loading_hints();
  resource_loading_hint1->set_loading_optimization_type(
      optimization_guide::proto::LOADING_BLOCK_RESOURCE);
  resource_loading_hint1->set_resource_pattern("news_cruft.js");

  // Page hint for "football"
  optimization_guide::proto::PageHint* page_hint2 = hint1->add_page_hints();
  page_hint2->set_page_pattern("football");
  optimization_guide::proto::Optimization* optimization2 =
      page_hint2->add_whitelisted_optimizations();
  optimization2->set_optimization_type(
      optimization_guide::proto::RESOURCE_LOADING);
  optimization_guide::proto::ResourceLoadingHint* resource_loading_hint2 =
      optimization2->add_resource_loading_hints();
  resource_loading_hint2->set_loading_optimization_type(
      optimization_guide::proto::LOADING_BLOCK_RESOURCE);
  resource_loading_hint2->set_resource_pattern("football_cruft.js");

  optimization_guide::proto::ResourceLoadingHint* resource_loading_hint3 =
      optimization2->add_resource_loading_hints();
  resource_loading_hint3->set_loading_optimization_type(
      optimization_guide::proto::LOADING_BLOCK_RESOURCE);
  resource_loading_hint3->set_resource_pattern("barball_cruft.js");
  ProcessHints(config, "2.0.0");

  RunUntilIdle();
}

void PreviewsOptimizationGuideTest::InitializeMultipleResourceLoadingHints(
    size_t key_count,
    size_t page_patterns_per_key) {
  optimization_guide::proto::Configuration config;

  for (size_t key_index = 0; key_index < key_count; ++key_index) {
    optimization_guide::proto::Hint* hint = config.add_hints();
    hint->set_key("somedomain" + base::NumberToString(key_index) + ".org");
    hint->set_key_representation(optimization_guide::proto::HOST_SUFFIX);

    for (size_t page_pattern_index = 0;
         page_pattern_index < page_patterns_per_key; ++page_pattern_index) {
      // Page hint for "/news/"
      optimization_guide::proto::PageHint* page_hint = hint->add_page_hints();
      page_hint->set_page_pattern("/news/" +
                                  base::NumberToString(page_pattern_index));
      optimization_guide::proto::Optimization* optimization1 =
          page_hint->add_whitelisted_optimizations();
      optimization1->set_optimization_type(
          optimization_guide::proto::RESOURCE_LOADING);

      optimization_guide::proto::ResourceLoadingHint* resource_loading_hint_1 =
          optimization1->add_resource_loading_hints();
      resource_loading_hint_1->set_loading_optimization_type(
          optimization_guide::proto::LOADING_BLOCK_RESOURCE);
      resource_loading_hint_1->set_resource_pattern("news_cruft_1.js");

      optimization_guide::proto::ResourceLoadingHint* resource_loading_hint_2 =
          optimization1->add_resource_loading_hints();
      resource_loading_hint_2->set_loading_optimization_type(
          optimization_guide::proto::LOADING_BLOCK_RESOURCE);
      resource_loading_hint_2->set_resource_pattern("news_cruft_2.js");
    }
  }
  ProcessHints(config, "2.0.0");

  RunUntilIdle();
}

void PreviewsOptimizationGuideTest::InitializeWithLitePageRedirectBlacklist() {
  previews::BloomFilter blacklist_bloom_filter(7, 511);
  blacklist_bloom_filter.Add("blacklisteddomain.com");
  blacklist_bloom_filter.Add("blacklistedsubdomain.maindomain.co.in");
  std::string blacklist_data((char*)&blacklist_bloom_filter.bytes()[0],
                             blacklist_bloom_filter.bytes().size());
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::OptimizationFilter* blacklist_proto =
      config.add_optimization_blacklists();
  blacklist_proto->set_optimization_type(
      optimization_guide::proto::LITE_PAGE_REDIRECT);
  std::unique_ptr<optimization_guide::proto::BloomFilter> bloom_filter_proto =
      std::make_unique<optimization_guide::proto::BloomFilter>();
  bloom_filter_proto->set_num_hash_functions(7);
  bloom_filter_proto->set_num_bits(511);
  bloom_filter_proto->set_data(blacklist_data);
  blacklist_proto->set_allocated_bloom_filter(bloom_filter_proto.release());
  ProcessHints(config, "2.0.0");

  RunUntilIdle();
}

TEST_F(PreviewsOptimizationGuideTest, IsWhitelistedWithoutHints) {
  PreviewsUserData user_data(kDefaultPageId);
  EXPECT_FALSE(guide()->IsWhitelisted(
      &user_data, GURL("https://m.facebook.com"), PreviewsType::NOSCRIPT));
}

TEST_F(PreviewsOptimizationGuideTest,
       ProcessHintsWhitelistForNoScriptPopulatedCorrectly) {
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint1 = config.add_hints();
  hint1->set_key("facebook.com");
  hint1->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::Optimization* optimization1 =
      hint1->add_whitelisted_optimizations();
  optimization1->set_optimization_type(optimization_guide::proto::NOSCRIPT);
  // Add a second optimization to ensure that the applicable optimizations are
  // still whitelisted.
  optimization_guide::proto::Optimization* optimization2 =
      hint1->add_whitelisted_optimizations();
  optimization2->set_optimization_type(
      optimization_guide::proto::TYPE_UNSPECIFIED);
  // Add a second hint.
  optimization_guide::proto::Hint* hint2 = config.add_hints();
  hint2->set_key("twitter.com");
  hint2->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::Optimization* optimization3 =
      hint2->add_whitelisted_optimizations();
  optimization3->set_optimization_type(optimization_guide::proto::NOSCRIPT);
  ProcessHints(config, "2.0.0");

  RunUntilIdle();
  PreviewsUserData user_data(kDefaultPageId);
  // Twitter and Facebook should be whitelisted but not Google.
  EXPECT_TRUE(guide()->IsWhitelisted(&user_data, GURL("https://m.facebook.com"),
                                     PreviewsType::NOSCRIPT));
  EXPECT_TRUE(guide()->IsWhitelisted(&user_data,
                                     GURL("https://m.twitter.com/example"),
                                     PreviewsType::NOSCRIPT));
  EXPECT_FALSE(guide()->IsWhitelisted(&user_data, GURL("https://google.com"),
                                      PreviewsType::NOSCRIPT));
}

// Test when resource loading hints are enabled.
TEST_F(PreviewsOptimizationGuideTest,
       ProcessHintsWhitelistForResourceLoadingHintsPopulatedCorrectly) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(features::kResourceLoadingHints);

  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint1 = config.add_hints();
  hint1->set_key("facebook.com");
  hint1->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint1 = hint1->add_page_hints();
  page_hint1->set_page_pattern("*");
  optimization_guide::proto::Optimization* optimization1 =
      page_hint1->add_whitelisted_optimizations();
  optimization1->set_optimization_type(
      optimization_guide::proto::RESOURCE_LOADING);

  // Add additional optimizations to ensure that the applicable optimizations
  // are still whitelisted.
  optimization_guide::proto::Optimization* optimization2 =
      hint1->add_whitelisted_optimizations();
  optimization2->set_optimization_type(
      optimization_guide::proto::TYPE_UNSPECIFIED);

  optimization_guide::proto::PageHint* page_hint2 = hint1->add_page_hints();
  page_hint2->set_page_pattern("*");
  optimization_guide::proto::Optimization* optimization3 =
      page_hint2->add_whitelisted_optimizations();
  optimization3->set_optimization_type(optimization_guide::proto::NOSCRIPT);

  // Add a second hint.
  optimization_guide::proto::Hint* hint2 = config.add_hints();
  hint2->set_key("twitter.com");
  hint2->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint3 = hint2->add_page_hints();
  page_hint3->set_page_pattern("*");
  optimization_guide::proto::Optimization* optimization4 =
      page_hint3->add_whitelisted_optimizations();
  optimization4->set_optimization_type(
      optimization_guide::proto::RESOURCE_LOADING);

  ProcessHints(config, "2.0.0");

  RunUntilIdle();

  PreviewsUserData user_data(kDefaultPageId);
  // Twitter and Facebook should be whitelisted but not Google.
  EXPECT_TRUE(guide()->IsWhitelisted(&user_data, GURL("https://m.facebook.com"),
                                     PreviewsType::RESOURCE_LOADING_HINTS));
  EXPECT_TRUE(guide()->IsWhitelisted(&user_data,
                                     GURL("https://m.twitter.com/example"),
                                     PreviewsType::RESOURCE_LOADING_HINTS));
  EXPECT_FALSE(guide()->IsWhitelisted(&user_data, GURL("https://google.com"),
                                      PreviewsType::RESOURCE_LOADING_HINTS));
}

// Test when both NoScript and resource loading hints are enabled.
TEST_F(
    PreviewsOptimizationGuideTest,
    ProcessHintsWhitelistForNoScriptAndResourceLoadingHintsPopulatedCorrectly) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(features::kResourceLoadingHints);

  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint1 = config.add_hints();
  hint1->set_key("facebook.com");
  hint1->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::Optimization* optimization1 =
      hint1->add_whitelisted_optimizations();
  optimization1->set_optimization_type(optimization_guide::proto::NOSCRIPT);

  // Add a second optimization to ensure that the applicable optimizations are
  // still whitelisted.
  optimization_guide::proto::Optimization* optimization2 =
      hint1->add_whitelisted_optimizations();
  optimization2->set_optimization_type(
      optimization_guide::proto::TYPE_UNSPECIFIED);

  // Add a second hint.
  optimization_guide::proto::Hint* hint2 = config.add_hints();
  hint2->set_key("twitter.com");
  hint2->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint1 = hint2->add_page_hints();
  page_hint1->set_page_pattern("*");
  optimization_guide::proto::Optimization* optimization3 =
      page_hint1->add_whitelisted_optimizations();
  optimization3->set_optimization_type(
      optimization_guide::proto::RESOURCE_LOADING);

  ProcessHints(config, "2.0.0");

  RunUntilIdle();
  PreviewsUserData user_data(kDefaultPageId);
  // Twitter and Facebook should be whitelisted but not Google.
  EXPECT_TRUE(guide()->IsWhitelisted(&user_data, GURL("https://m.facebook.com"),
                                     PreviewsType::NOSCRIPT));
  EXPECT_TRUE(guide()->IsWhitelisted(
      &user_data, GURL("https://m.facebook.com/example.html"),
      PreviewsType::NOSCRIPT));
  EXPECT_TRUE(guide()->IsWhitelisted(&user_data,
                                     GURL("https://m.twitter.com/example"),
                                     PreviewsType::RESOURCE_LOADING_HINTS));
  EXPECT_FALSE(guide()->IsWhitelisted(&user_data, GURL("https://google.com"),
                                      PreviewsType::RESOURCE_LOADING_HINTS));
}

// This is a helper function for testing the experiment flags on the config for
// the optimization guide. It creates a test config with a hint containing
// multiple optimizations. The optimization under test will be marked with an
// experiment name if one is provided in |experiment_name|. It will then be
// tested to see if it's enabled, the expectation found in |expect_enabled|.
void PreviewsOptimizationGuideTest::DoExperimentFlagTest(
    base::Optional<std::string> experiment_name,
    bool expect_enabled) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(features::kResourceLoadingHints);

  optimization_guide::proto::Configuration config;

  // Create a hint with two optimizations. One may be marked experimental
  // depending on test configuration. The other is never marked experimental.
  optimization_guide::proto::Hint* hint1 = config.add_hints();
  hint1->set_key("facebook.com");
  hint1->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::Optimization* optimization1 =
      hint1->add_whitelisted_optimizations();
  // NOSCRIPT is the optimization under test and may be marked experimental.
  if (experiment_name.has_value()) {
    optimization1->set_experiment_name(experiment_name.value());
  }
  optimization1->set_optimization_type(optimization_guide::proto::NOSCRIPT);

  // RESOURCE_LOADING is never marked experimental.
  optimization_guide::proto::PageHint* page_hint1 = hint1->add_page_hints();
  page_hint1->set_page_pattern("*");
  optimization_guide::proto::Optimization* optimization2 =
      page_hint1->add_whitelisted_optimizations();
  optimization2->set_optimization_type(
      optimization_guide::proto::RESOURCE_LOADING);

  // Add a second, non-experimental hint.
  optimization_guide::proto::Hint* hint2 = config.add_hints();
  hint2->set_key("twitter.com");
  hint2->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::Optimization* optimization3 =
      hint2->add_whitelisted_optimizations();
  optimization3->set_optimization_type(optimization_guide::proto::NOSCRIPT);
  ProcessHints(config, "2.0.0");

  RunUntilIdle();

  PreviewsUserData user_data(kDefaultPageId);
  // Check to ensure the optimization under test (facebook noscript) is either
  // enabled or disabled, depending on what the caller told us to expect.
  EXPECT_EQ(expect_enabled,
            guide()->IsWhitelisted(&user_data, GURL("https://m.facebook.com"),
                                   PreviewsType::NOSCRIPT));

  // RESOURCE_LOADING_HINTS for facebook should always be enabled.
  EXPECT_TRUE(guide()->IsWhitelisted(&user_data, GURL("https://m.facebook.com"),
                                     PreviewsType::RESOURCE_LOADING_HINTS));
  // Twitter's NOSCRIPT should always be enabled; RESOURCE_LOADING_HINTS is not
  // configured and should be disabled.
  EXPECT_TRUE(guide()->IsWhitelisted(&user_data,
                                     GURL("https://m.twitter.com/example"),
                                     PreviewsType::NOSCRIPT));
  // Google (which is not configured at all) should always have both NOSCRIPT
  // and RESOURCE_LOADING_HINTS disabled.
  EXPECT_FALSE(guide()->IsWhitelisted(&user_data, GURL("https://google.com"),
                                      PreviewsType::NOSCRIPT));
}

TEST_F(PreviewsOptimizationGuideTest,
       HandlesExperimentalFlagWithNoExperimentFlaggedOrEnabled) {
  // With the optimization NOT flagged as experimental and no experiment
  // enabled, the optimization should be enabled.
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndDisableFeature(features::kOptimizationHintsExperiments);
  DoExperimentFlagTest(base::nullopt, true);
}

TEST_F(PreviewsOptimizationGuideTest,
       HandlesExperimentalFlagWithEmptyExperimentName) {
  // Empty experiment names should be equivalent to no experiment flag set.
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndDisableFeature(features::kOptimizationHintsExperiments);
  DoExperimentFlagTest("", true);
}

TEST_F(PreviewsOptimizationGuideTest,
       HandlesExperimentalFlagWithExperimentConfiguredAndNotRunning) {
  // With the optimization flagged as experimental and no experiment
  // enabled, the optimization should be disabled.
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndDisableFeature(features::kOptimizationHintsExperiments);
  DoExperimentFlagTest("foo_experiment", false);
}

TEST_F(PreviewsOptimizationGuideTest,
       HandlesExperimentalFlagWithExperimentConfiguredAndSameOneRunning) {
  // With the optimization flagged as experimental and an experiment with that
  // name running, the optimization should be enabled.
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeatureWithParameters(
      features::kOptimizationHintsExperiments,
      {{"experiment_name", "foo_experiment"}});
  DoExperimentFlagTest("foo_experiment", true);
}

TEST_F(PreviewsOptimizationGuideTest,
       HandlesExperimentalFlagWithExperimentConfiguredAndDifferentOneRunning) {
  // With the optimization flagged as experimental and a *different* experiment
  // enabled, the optimization should be disabled.
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeatureWithParameters(
      features::kOptimizationHintsExperiments,
      {{"experiment_name", "bar_experiment"}});
  DoExperimentFlagTest("foo_experiment", false);
}

TEST_F(PreviewsOptimizationGuideTest, EnsureExperimentsDisabledByDefault) {
  // Mark an optimization as experiment, and ensure it's disabled even though we
  // don't explicitly enable or disable the feature as part of the test. This
  // ensures the experiments feature is disabled by default.
  DoExperimentFlagTest("foo_experiment", false);
}

TEST_F(PreviewsOptimizationGuideTest, ProcessHintsUnsupportedKeyRepIsIgnored) {
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint = config.add_hints();
  hint->set_key("facebook.com");
  hint->set_key_representation(
      optimization_guide::proto::REPRESENTATION_UNSPECIFIED);
  optimization_guide::proto::Optimization* optimization =
      hint->add_whitelisted_optimizations();
  optimization->set_optimization_type(optimization_guide::proto::NOSCRIPT);
  ProcessHints(config, "2.0.0");

  RunUntilIdle();

  PreviewsUserData user_data(kDefaultPageId);
  EXPECT_FALSE(guide()->IsWhitelisted(
      &user_data, GURL("https://m.facebook.com"), PreviewsType::NOSCRIPT));
}

TEST_F(PreviewsOptimizationGuideTest,
       ProcessHintsUnsupportedOptimizationIsIgnored) {
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint = config.add_hints();
  hint->set_key("facebook.com");
  hint->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::Optimization* optimization =
      hint->add_whitelisted_optimizations();
  optimization->set_optimization_type(
      optimization_guide::proto::TYPE_UNSPECIFIED);
  ProcessHints(config, "2.0.0");

  RunUntilIdle();

  PreviewsUserData user_data(kDefaultPageId);

  EXPECT_FALSE(guide()->IsWhitelisted(
      &user_data, GURL("https://m.facebook.com"), PreviewsType::NOSCRIPT));
}

TEST_F(PreviewsOptimizationGuideTest, ProcessHintsWithExistingSentinel) {
  base::HistogramTester histogram_tester;

  // Create valid config.
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint1 = config.add_hints();
  hint1->set_key("facebook.com");
  hint1->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::Optimization* optimization1 =
      hint1->add_whitelisted_optimizations();
  optimization1->set_optimization_type(optimization_guide::proto::NOSCRIPT);

  // Create sentinel file for version 2.0.0.
  const base::FilePath sentinel_path =
      temp_dir().Append(FILE_PATH_LITERAL("previews_config_sentinel.txt"));
  base::WriteFile(sentinel_path, "2.0.0", 5);

  // Verify config not processed for version 2.0.0 (same as sentinel).
  ProcessHints(config, "2.0.0");
  RunUntilIdle();
  PreviewsUserData user_data(kDefaultPageId);
  EXPECT_FALSE(guide()->IsWhitelisted(
      &user_data, GURL("https://m.facebook.com"), PreviewsType::NOSCRIPT));
  EXPECT_TRUE(base::PathExists(sentinel_path));
  histogram_tester.ExpectUniqueSample("Previews.ProcessHintsResult",
                                      2 /* FAILED_FINISH_PROCESSING */, 1);

  // Now verify config is processed for different version and sentinel cleared.
  ProcessHints(config, "3.0.0");
  RunUntilIdle();
  EXPECT_TRUE(guide()->IsWhitelisted(&user_data, GURL("https://m.facebook.com"),
                                     PreviewsType::NOSCRIPT));
  EXPECT_FALSE(base::PathExists(sentinel_path));
  histogram_tester.ExpectBucketCount("Previews.ProcessHintsResult",
                                     1 /* PROCESSED_PREVIEWS_HINTS */, 1);
}

TEST_F(PreviewsOptimizationGuideTest, ProcessHintsWithInvalidSentinelFile) {
  base::HistogramTester histogram_tester;

  // Create valid config.
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint1 = config.add_hints();
  hint1->set_key("facebook.com");
  hint1->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::Optimization* optimization1 =
      hint1->add_whitelisted_optimizations();
  optimization1->set_optimization_type(optimization_guide::proto::NOSCRIPT);

  // Create sentinel file with invalid contents.
  const base::FilePath sentinel_path =
      temp_dir().Append(FILE_PATH_LITERAL("previews_config_sentinel.txt"));
  base::WriteFile(sentinel_path, "bad-2.0.0", 5);

  // Verify config not processed for existing sentinel with bad value but
  // that the existinel sentinel file is deleted.
  ProcessHints(config, "2.0.0");
  RunUntilIdle();
  PreviewsUserData user_data(kDefaultPageId);
  EXPECT_FALSE(guide()->IsWhitelisted(
      &user_data, GURL("https://m.facebook.com"), PreviewsType::NOSCRIPT));
  EXPECT_FALSE(base::PathExists(sentinel_path));
  histogram_tester.ExpectUniqueSample("Previews.ProcessHintsResult",
                                      2 /* FAILED_FINISH_PROCESSING */, 1);

  // Now verify config is processed with sentinel cleared.
  ProcessHints(config, "2.0.0");
  RunUntilIdle();
  EXPECT_TRUE(guide()->IsWhitelisted(&user_data, GURL("https://m.facebook.com"),
                                     PreviewsType::NOSCRIPT));
  EXPECT_FALSE(base::PathExists(sentinel_path));
  histogram_tester.ExpectBucketCount("Previews.ProcessHintsResult",
                                     1 /* PROCESSED_PREVIEWS_HINTS */, 1);
}

TEST_F(PreviewsOptimizationGuideTest, ProcessHintConfigWithNoKeyFailsDcheck) {
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint = config.add_hints();
  hint->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::Optimization* optimization =
      hint->add_whitelisted_optimizations();
  optimization->set_optimization_type(optimization_guide::proto::NOSCRIPT);

  EXPECT_DCHECK_DEATH({
    ProcessHints(config, "2.0.0");
    RunUntilIdle();
  });
}

TEST_F(PreviewsOptimizationGuideTest,
       ProcessHintsConfigWithDuplicateKeysFailsDcheck) {
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint1 = config.add_hints();
  hint1->set_key("facebook.com");
  hint1->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::Optimization* optimization1 =
      hint1->add_whitelisted_optimizations();
  optimization1->set_optimization_type(optimization_guide::proto::NOSCRIPT);
  optimization_guide::proto::Hint* hint2 = config.add_hints();
  hint2->set_key("facebook.com");
  hint2->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::Optimization* optimization2 =
      hint2->add_whitelisted_optimizations();
  optimization2->set_optimization_type(optimization_guide::proto::NOSCRIPT);

  EXPECT_DCHECK_DEATH({
    ProcessHints(config, "2.0.0");
    RunUntilIdle();
  });
}

TEST_F(PreviewsOptimizationGuideTest, IsWhitelistedWithMultipleHintMatches) {
  optimization_guide::proto::Configuration config;

  // Whitelist NoScript for indoor.sports.yahoo.com:
  optimization_guide::proto::Hint* hint1 = config.add_hints();
  hint1->set_key("indoor.sports.yahoo.com");
  hint1->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::Optimization* optimization1 =
      hint1->add_whitelisted_optimizations();
  optimization1->set_optimization_type(optimization_guide::proto::NOSCRIPT);
  optimization1->set_inflation_percent(10);

  // No optimizations for sports.yahoo.com:
  optimization_guide::proto::Hint* hint2 = config.add_hints();
  hint2->set_key("sports.yahoo.com");
  hint2->set_key_representation(optimization_guide::proto::HOST_SUFFIX);

  // Whitelist NoScript for base domain yahoo.com:
  optimization_guide::proto::Hint* hint3 = config.add_hints();
  hint3->set_key("yahoo.com");
  hint3->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::Optimization* optimization3 =
      hint3->add_whitelisted_optimizations();
  optimization3->set_optimization_type(optimization_guide::proto::NOSCRIPT);
  optimization3->set_inflation_percent(30);

  // No optimizations for mail.yahoo.com:
  optimization_guide::proto::Hint* hint4 = config.add_hints();
  hint4->set_key("mail.yahoo.com");
  hint4->set_key_representation(optimization_guide::proto::HOST_SUFFIX);

  ProcessHints(config, "2.0.0");
  RunUntilIdle();

  PreviewsUserData user_data(1);
  EXPECT_TRUE(guide()->IsWhitelisted(&user_data, GURL("https://yahoo.com"),
                                     PreviewsType::NOSCRIPT));
  EXPECT_EQ(30, user_data.data_savings_inflation_percent());

  PreviewsUserData user_data2(2);
  // Uses "sports.yahoo.com" match before "yahoo.com" match.
  EXPECT_FALSE(guide()->IsWhitelisted(
      &user_data, GURL("https://sports.yahoo.com"), PreviewsType::NOSCRIPT));

  PreviewsUserData user_data3(3);
  // Uses "yahoo.com" match before "mail.yahoo.com" match.
  EXPECT_TRUE(guide()->IsWhitelisted(
      &user_data3, GURL("https://mail.yahoo.com"), PreviewsType::NOSCRIPT));
  EXPECT_EQ(30, user_data3.data_savings_inflation_percent());

  PreviewsUserData user_data4(4);
  // Uses "indoor.sports.yahoo.com" match before "sports.yahoo.com" match.
  EXPECT_TRUE(guide()->IsWhitelisted(&user_data4,
                                     GURL("https://indoor.sports.yahoo.com"),
                                     PreviewsType::NOSCRIPT));
  EXPECT_EQ(10, user_data4.data_savings_inflation_percent());

  PreviewsUserData user_data5(5);
  // Uses "sports.yahoo.com" match before "yahoo.com" match.
  EXPECT_FALSE(guide()->IsWhitelisted(&user_data5,
                                      GURL("https://outdoor.sports.yahoo.com"),
                                      PreviewsType::NOSCRIPT));
}

TEST_F(PreviewsOptimizationGuideTest, MaybeLoadOptimizationHints) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(features::kResourceLoadingHints);

  InitializeFixedCountResourceLoadingHints();

  EXPECT_TRUE(guide()->MaybeLoadOptimizationHints(
      GURL("https://somedomain.org/"), base::DoNothing()));
  EXPECT_TRUE(guide()->MaybeLoadOptimizationHints(
      GURL("https://www.somedomain.org/news/football"),
      base::BindOnce(
          &PreviewsOptimizationGuideTest::MaybeLoadOptimizationHintsCallback,
          base::Unretained(this))));
  EXPECT_FALSE(guide()->MaybeLoadOptimizationHints(
      GURL("https://www.unknown.com"), base::DoNothing()));

  RunUntilIdle();
  histogram_tester.ExpectUniqueSample(
      "ResourceLoadingHints.PageHints.ProcessedCount", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "ResourceLoadingHints.ResourceHints.TotalReceived", 3, 1);
  histogram_tester.ExpectUniqueSample(
      "ResourceLoadingHints.PageHints.TotalReceived", 2, 1);

  // Verify loaded hint data for www.somedomain.org
  EXPECT_EQ(GURL("https://www.somedomain.org/news/football"),
            loaded_hints_document_gurl());
  EXPECT_EQ(1ul, loaded_hints_resource_patterns().size());
  EXPECT_EQ("news_cruft.js", loaded_hints_resource_patterns().front());

  PreviewsUserData user_data(kDefaultPageId);
  // Verify whitelisting from loaded page hints.
  EXPECT_TRUE(guide()->IsWhitelisted(
      &user_data,
      GURL("https://www.somedomain.org/news/weather/raininginseattle"),
      PreviewsType::RESOURCE_LOADING_HINTS));
  EXPECT_TRUE(guide()->IsWhitelisted(
      &user_data,
      GURL("https://www.somedomain.org/football/seahawksrebuildingyear"),
      PreviewsType::RESOURCE_LOADING_HINTS));
  EXPECT_FALSE(guide()->IsWhitelisted(
      &user_data, GURL("https://www.somedomain.org/unhinted"),
      PreviewsType::RESOURCE_LOADING_HINTS));
}

// Test that optimization hints with multiple page patterns is processed
// correctly.
TEST_F(PreviewsOptimizationGuideTest,
       LoadManyResourceLoadingOptimizationHints) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(features::kResourceLoadingHints);

  const size_t key_count = 20;
  const size_t page_patterns_per_key = 25;

  ASSERT_EQ(previews::params::GetMaxPageHintsInMemoryThreshhold(),
            key_count * page_patterns_per_key);

  // Count of page patterns is within the threshold.
  ASSERT_LE(key_count * page_patterns_per_key,
            previews::params::GetMaxPageHintsInMemoryThreshhold());

  InitializeMultipleResourceLoadingHints(key_count, page_patterns_per_key);

  EXPECT_TRUE(guide()->MaybeLoadOptimizationHints(
      GURL("https://somedomain0.org/"), base::DoNothing()));
  EXPECT_TRUE(guide()->MaybeLoadOptimizationHints(
      GURL("https://www.somedomain0.org/news0/football"),
      base::BindOnce(
          &PreviewsOptimizationGuideTest::MaybeLoadOptimizationHintsCallback,
          base::Unretained(this))));
  EXPECT_TRUE(guide()->MaybeLoadOptimizationHints(

      GURL("https://www.somedomain0.org/news499/football"),
      base::BindOnce(
          &PreviewsOptimizationGuideTest::MaybeLoadOptimizationHintsCallback,
          base::Unretained(this))));
  EXPECT_TRUE(guide()->MaybeLoadOptimizationHints(

      GURL("https://www.somedomain0.org/news500/football"),
      base::BindOnce(
          &PreviewsOptimizationGuideTest::MaybeLoadOptimizationHintsCallback,
          base::Unretained(this))));

  EXPECT_TRUE(guide()->MaybeLoadOptimizationHints(
      GURL("https://somedomain19.org/"), base::DoNothing()));
  EXPECT_FALSE(guide()->MaybeLoadOptimizationHints(
      GURL("https://somedomain20.org/"), base::DoNothing()));
  EXPECT_FALSE(guide()->MaybeLoadOptimizationHints(
      GURL("https://www.unknown.com"), base::DoNothing()));

  RunUntilIdle();
  histogram_tester.ExpectUniqueSample(
      "ResourceLoadingHints.PageHints.ProcessedCount", page_patterns_per_key,
      key_count);
  histogram_tester.ExpectUniqueSample(
      "ResourceLoadingHints.ResourceHints.TotalReceived",
      key_count * page_patterns_per_key * 2, 1);
  histogram_tester.ExpectUniqueSample(
      "ResourceLoadingHints.PageHints.TotalReceived",
      key_count * page_patterns_per_key, 1);
}

// Test that only up to GetMaxPageHintsInMemoryThreshhold() page hints
// are loaded to the memory.
TEST_F(PreviewsOptimizationGuideTest,
       LoadTooManyResourceLoadingOptimizationHints) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(features::kResourceLoadingHints);

  const size_t key_count = 21;
  const size_t page_patterns_per_key = 25;

  ASSERT_EQ(previews::params::GetMaxPageHintsInMemoryThreshhold(),
            20u * page_patterns_per_key);

  // Provide more page patterns than the threshold.
  ASSERT_GT(key_count * page_patterns_per_key,
            previews::params::GetMaxPageHintsInMemoryThreshhold());

  InitializeMultipleResourceLoadingHints(key_count, page_patterns_per_key);

  EXPECT_TRUE(guide()->MaybeLoadOptimizationHints(
      GURL("https://somedomain0.org/"), base::DoNothing()));
  EXPECT_TRUE(guide()->MaybeLoadOptimizationHints(
      GURL("https://www.somedomain0.org/news0/football"),
      base::BindOnce(
          &PreviewsOptimizationGuideTest::MaybeLoadOptimizationHintsCallback,
          base::Unretained(this))));

  EXPECT_TRUE(guide()->MaybeLoadOptimizationHints(
      GURL("https://somedomain19.org/"), base::DoNothing()));
  EXPECT_TRUE(guide()->MaybeLoadOptimizationHints(

      GURL("https://www.somedomain19.org/news0/football"),
      base::BindOnce(
          &PreviewsOptimizationGuideTest::MaybeLoadOptimizationHintsCallback,
          base::Unretained(this))));

  // The last page pattern should be dropped since it exceeds the threshold
  // count.
  EXPECT_FALSE(guide()->MaybeLoadOptimizationHints(
      GURL("https://somedomain20.org/"), base::DoNothing()));
  EXPECT_FALSE(guide()->MaybeLoadOptimizationHints(

      GURL("https://www.somedomain20.org/news0/football"),
      base::BindOnce(
          &PreviewsOptimizationGuideTest::MaybeLoadOptimizationHintsCallback,
          base::Unretained(this))));

  RunUntilIdle();
  histogram_tester.ExpectUniqueSample(
      "ResourceLoadingHints.PageHints.ProcessedCount", page_patterns_per_key,
      key_count);
  histogram_tester.ExpectUniqueSample(
      "ResourceLoadingHints.ResourceHints.TotalReceived",
      key_count * page_patterns_per_key * 2, 1);
  histogram_tester.ExpectUniqueSample(
      "ResourceLoadingHints.PageHints.TotalReceived",
      key_count * page_patterns_per_key, 1);
}

TEST_F(PreviewsOptimizationGuideTest,
       MaybeLoadOptimizationHintsWithoutEnabledPageHintsFeature) {
  // Without PageHints-oriented feature enabled, never see
  // enabled, the optimization should be disabled.
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndDisableFeature(features::kResourceLoadingHints);

  InitializeFixedCountResourceLoadingHints();

  EXPECT_FALSE(guide()->MaybeLoadOptimizationHints(
      GURL("https://www.somedomain.org"), base::DoNothing()));

  RunUntilIdle();
  PreviewsUserData user_data(kDefaultPageId);
  EXPECT_FALSE(guide()->IsWhitelisted(
      &user_data,
      GURL("https://www.somedomain.org/news/weather/raininginseattle"),
      PreviewsType::RESOURCE_LOADING_HINTS));
}

TEST_F(PreviewsOptimizationGuideTest, IsBlacklisted) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(features::kLitePageServerPreviews);

  EXPECT_FALSE(
      guide()->IsBlacklisted(GURL("https://m.blacklisteddomain.com/path"),
                             PreviewsType::LITE_PAGE_REDIRECT));

  InitializeWithLitePageRedirectBlacklist();

  EXPECT_TRUE(
      guide()->IsBlacklisted(GURL("https://m.blacklisteddomain.com/path"),
                             PreviewsType::LITE_PAGE_REDIRECT));
  EXPECT_FALSE(guide()->IsBlacklisted(
      GURL("https://m.blacklisteddomain.com/path"), PreviewsType::NOSCRIPT));

  EXPECT_TRUE(guide()->IsBlacklisted(
      GURL("https://blacklistedsubdomain.maindomain.co.in"),
      PreviewsType::LITE_PAGE_REDIRECT));

  EXPECT_FALSE(guide()->IsBlacklisted(GURL("https://maindomain.co.in"),
                                      PreviewsType::LITE_PAGE_REDIRECT));
}

TEST_F(PreviewsOptimizationGuideTest,
       IsBlacklistedWithLitePageServerPreviewsDisabled) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndDisableFeature(features::kLitePageServerPreviews);

  InitializeWithLitePageRedirectBlacklist();

  EXPECT_FALSE(
      guide()->IsBlacklisted(GURL("https://m.blacklisteddomain.com/path"),
                             PreviewsType::LITE_PAGE_REDIRECT));
}

TEST_F(PreviewsOptimizationGuideTest, RemoveObserverCalledAtDestruction) {
  EXPECT_FALSE(optimization_guide_service()->RemoveObserverCalled());

  ResetGuide();

  EXPECT_TRUE(optimization_guide_service()->RemoveObserverCalled());
}

}  // namespace previews
