// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/distiller_page.h"

#include <optional>

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/dom_distiller/core/dom_distiller_constants.h"
#include "components/dom_distiller/core/dom_distiller_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/dom_distiller_js/dom_distiller.pb.h"
#include "url/gurl.h"

namespace dom_distiller {

namespace {

enum class DistillationParseResult {
  kSuccess = 0,
  kParseFailure = 1,
  kNoResult = 2,
  kContentTooShort = 3,
};

constexpr char kReadabilityTitle[] = "title";
constexpr char kReadabilityContent[] = "content";
constexpr char kReadabilityDir[] = "dir";
constexpr char kReadabilityTextContent[] = "textContent";

// This is a minimal concrete class that inherits from DistillerPage. It
// overrides the implementation of distillation to immediately call the
// completion handler, simulating a success or failure result for the test.
class TestDistillerPage : public DistillerPage {
 public:
  // This enum controls which distillation outcome the mock will simulate.
  enum class SimulatedResult {
    kSuccess,
    kParseFailure,
    kNoResult,
    kCustomResult,
    kNullResult,
  };

  TestDistillerPage() = default;

  // Configures the mock to simulate a specific result.
  void SetNextResult(SimulatedResult result) { simulate_result_ = result; }

  // Configures the mock to simulate a specific result value.
  void SetNextResultValue(std::optional<base::Value> val) {
    simulate_result_ = SimulatedResult::kCustomResult;
    simulate_result_val_ = std::move(val);
  }

  bool ShouldFetchOfflineData() override { return false; }

  DistillerType GetDistillerType() override {
    return ShouldUseReadabilityDistiller() ? DistillerType::kReadability
                                           : DistillerType::kDOMDistiller;
  }

  // The overridden implementation now simulates one of three outcomes based on
  // the configuration set by SetNextResult().
  void DistillPageImpl(const GURL& url, const std::string& script) override {
    switch (simulate_result_) {
      case SimulatedResult::kSuccess: {
        // A valid (but empty) dictionary simulates a parsable result from the
        // distiller, leading to `kSuccess`.
        const base::Value success_value(base::Value::Type::DICT);
        OnDistillationDone(url, &success_value);
        break;
      }
      case SimulatedResult::kParseFailure: {
        // An invalid type (e.g., a string instead of a dict) will cause a
        // parse failure, leading to `kParseFailure`.
        const base::Value parse_failure_value("not a dictionary");
        OnDistillationDone(url, &parse_failure_value);
        break;
      }
      case SimulatedResult::kNoResult: {
        // A NONE value simulates the distiller returning nothing, which leads
        // to `kNoResult`.
        const base::Value no_result_value(base::Value::Type::NONE);
        OnDistillationDone(url, &no_result_value);
        break;
      }
      case SimulatedResult::kCustomResult: {
        OnDistillationDone(url, &simulate_result_val_.value());
        break;
      }
      case SimulatedResult::kNullResult: {
        OnDistillationDone(url, nullptr);
        break;
      }
    }
  }

 private:
  SimulatedResult simulate_result_ = SimulatedResult::kSuccess;
  std::optional<base::Value> simulate_result_val_ = std::nullopt;
};

class DistillerPageTest : public testing::Test {
 protected:
  DistillerPageTest() {
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/{},
        /*disabled_features=*/{kReaderModeUseReadability});
  }

  base::test::ScopedFeatureList feature_list_;
  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
};

// Test that the kSuccess value is recorded when distillation is successful.
TEST_F(DistillerPageTest, RecordsSuccessMetric) {
  TestDistillerPage distiller_page;
  distiller_page.SetNextResult(TestDistillerPage::SimulatedResult::kSuccess);

  distiller_page.DistillPage(GURL("http://example.com/success"),
                             dom_distiller::proto::DomDistillerOptions(),
                             base::DoNothing());

  // Check that the UMA metric was recorded with the correct enum value.
  histogram_tester_.ExpectUniqueSample("DomDistiller.Distillation.Result",
                                       DistillationParseResult::kSuccess, 1);
}

// Test that the kParseFailure value is recorded when the result is unparsable.
TEST_F(DistillerPageTest, RecordsParseFailureMetric) {
  TestDistillerPage distiller_page;
  distiller_page.SetNextResult(
      TestDistillerPage::SimulatedResult::kParseFailure);

  distiller_page.DistillPage(GURL("http://example.com/failure"),
                             dom_distiller::proto::DomDistillerOptions(),
                             base::DoNothing());

  histogram_tester_.ExpectUniqueSample("DomDistiller.Distillation.Result",
                                       DistillationParseResult::kParseFailure,
                                       1);
}

// Test that the kNoResult value is recorded when the distiller returns nothing.
TEST_F(DistillerPageTest, RecordsNoResultMetric) {
  TestDistillerPage distiller_page;
  distiller_page.SetNextResult(TestDistillerPage::SimulatedResult::kNoResult);

  distiller_page.DistillPage(GURL("http://example.com/no-result"),
                             dom_distiller::proto::DomDistillerOptions(),
                             base::DoNothing());

  histogram_tester_.ExpectUniqueSample("DomDistiller.Distillation.Result",
                                       DistillationParseResult::kNoResult, 1);
}

// Test that the kNullResult value is recorded when the distiller returns null.
TEST_F(DistillerPageTest, RecordsNullResultMetric) {
  TestDistillerPage distiller_page;
  distiller_page.SetNextResult(TestDistillerPage::SimulatedResult::kNullResult);

  distiller_page.DistillPage(GURL("http://example.com/null-result"),
                             dom_distiller::proto::DomDistillerOptions(),
                             base::DoNothing());

  histogram_tester_.ExpectUniqueSample("DomDistiller.Distillation.Result",
                                       DistillationParseResult::kNoResult, 1);
}

// Asserts the fields exist in the DomDistillerResult.
void AssertCorrectDomDistillerResult(proto::DomDistillerResult& result,
                                     const std::string& title,
                                     const std::string& content,
                                     const std::string& dir,
                                     const int word_count) {
  ASSERT_EQ(title, result.title());
  ASSERT_EQ(content, result.distilled_content().html());
  ASSERT_EQ(dir, result.text_direction());
  ASSERT_EQ(word_count, result.statistics_info().word_count());
}

TEST_F(DistillerPageTest, ReadabilityObjectIsExtracted) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      /*enabled_features=*/{{dom_distiller::kReaderModeUseReadability,
                             {{"use_distiller", "true"}, {"min_content_length", "0"}}}},
      /*disabled_features=*/{});

  base::Value::Dict readability_result;
  const std::string title = "test_title";
  readability_result.Set(kReadabilityTitle, title);
  const std::string content = "test content";
  readability_result.Set(kReadabilityContent, content);
  const std::string dir = "ltr";
  readability_result.Set(kReadabilityDir, dir);
  const std::string text_content =
      "one two; three. four!  fivefive six, seven, eight nine ten";
  readability_result.Set(kReadabilityTextContent, text_content);
  TestDistillerPage distiller_page;
  distiller_page.SetNextResultValue(base::Value(std::move(readability_result)));

  base::RunLoop run_loop;
  DistillerPage::DistillerPageCallback cb =
      base::BindOnce(
          [](std::string title, std::string content, std::string dir,
             int word_count,
             std::unique_ptr<proto::DomDistillerResult> distilled_page,
             bool distillation_successful) {
            EXPECT_TRUE(distillation_successful);
            AssertCorrectDomDistillerResult(*distilled_page.get(), title,
                                            content, dir, 10);
          },
          title, content, dir, 10)
          .Then(run_loop.QuitClosure());
  distiller_page.DistillPage(GURL("http://example.com/success"),
                             dom_distiller::proto::DomDistillerOptions(),
                             std::move(cb));
  run_loop.Run();
  histogram_tester_.ExpectUniqueSample("DomDistiller.Distillation.Result",
                                       DistillationParseResult::kSuccess, 1);
}

TEST_F(DistillerPageTest,
       ReadabilityObjectIsExtracted_AutoDirWhenNoneProvided) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      /*enabled_features=*/{{dom_distiller::kReaderModeUseReadability,
                             {{"use_distiller", "true"}, {"min_content_length", "0"}}}},
      /*disabled_features=*/{});

  base::Value::Dict readability_result;
  const std::string title = "test_title";
  readability_result.Set(kReadabilityTitle, title);
  const std::string content = "test content";
  readability_result.Set(kReadabilityContent, content);
  const std::string dir = "auto";
  const std::string text_content =
      "one two; three. four!  fivefive six, seven, eight nine ten";
  readability_result.Set(kReadabilityTextContent, text_content);
  TestDistillerPage distiller_page;
  distiller_page.SetNextResultValue(base::Value(std::move(readability_result)));

  base::RunLoop run_loop;
  DistillerPage::DistillerPageCallback cb =
      base::BindOnce(
          [](std::string title, std::string content, std::string dir,
             int word_count,
             std::unique_ptr<proto::DomDistillerResult> distilled_page,
             bool distillation_successful) {
            EXPECT_TRUE(distillation_successful);
            AssertCorrectDomDistillerResult(*distilled_page.get(), title,
                                            content, dir, 10);
          },
          title, content, dir, 10)
          .Then(run_loop.QuitClosure());
  distiller_page.DistillPage(GURL("http://example.com/success"),
                             dom_distiller::proto::DomDistillerOptions(),
                             std::move(cb));
  run_loop.Run();
  histogram_tester_.ExpectUniqueSample("DomDistiller.Distillation.Result",
                                       DistillationParseResult::kSuccess, 1);
}

TEST_F(DistillerPageTest, ReadabilityObjectIsExtracted_FailureWhenNotDict) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      /*enabled_features=*/{{dom_distiller::kReaderModeUseReadability,
                             {{"use_distiller", "true"}, {"min_content_length", "0"}}}},
      /*disabled_features=*/{});

  base::Value readability_result("undefined");
  TestDistillerPage distiller_page;
  distiller_page.SetNextResultValue(base::Value(std::move(readability_result)));

  base::RunLoop run_loop;
  DistillerPage::DistillerPageCallback cb =
      base::BindOnce(
          [](std::unique_ptr<proto::DomDistillerResult> distilled_page,
             bool distillation_successful) {
            EXPECT_FALSE(distillation_successful);
          })
          .Then(run_loop.QuitClosure());
  distiller_page.DistillPage(GURL("http://example.com/success"),
                             dom_distiller::proto::DomDistillerOptions(),
                             std::move(cb));
  run_loop.Run();

  histogram_tester_.ExpectUniqueSample("DomDistiller.Distillation.Result",
                                       DistillationParseResult::kParseFailure,
                                       1);
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(DistillerPageTest, DistillationFailsWhenMinContentLengthNotMet) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      /*enabled_features=*/{{dom_distiller::kReaderModeUseReadability,
                             {{"use_distiller", "true"}, {"min_content_length", "1000"}}}},
      /*disabled_features=*/{});

  base::Value::Dict readability_result;
  const std::string title = "test_title";
  readability_result.Set(kReadabilityTitle, title);
  const std::string content = "test content";
  readability_result.Set(kReadabilityContent, content);
  const std::string dir = "ltr";
  readability_result.Set(kReadabilityDir, dir);
  const std::string text_content =
      "one two; three. four!  fivefive six, seven, eight nine ten";
  readability_result.Set(kReadabilityTextContent, text_content);
  TestDistillerPage distiller_page;
  distiller_page.SetNextResultValue(base::Value(std::move(readability_result)));

  base::RunLoop run_loop;
  DistillerPage::DistillerPageCallback cb =
      base::BindOnce(
          [](std::string title, std::string content, std::string dir,
             int word_count,
             std::unique_ptr<proto::DomDistillerResult> distilled_page,
             bool distillation_successful) {
            EXPECT_FALSE(distillation_successful);
          },
          title, content, dir, 10)
          .Then(run_loop.QuitClosure());
  distiller_page.DistillPage(GURL("http://example.com/success"),
                             dom_distiller::proto::DomDistillerOptions(),
                             std::move(cb));
  run_loop.Run();
  histogram_tester_.ExpectUniqueSample(
      "DomDistiller.Distillation.Result",
      DistillationParseResult::kContentTooShort, 1);
}
#endif

}  // namespace

}  // namespace dom_distiller
