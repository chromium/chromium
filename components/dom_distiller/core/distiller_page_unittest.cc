// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/distiller_page.h"

#include "base/functional/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/dom_distiller_js/dom_distiller.pb.h"
#include "url/gurl.h"

namespace dom_distiller {

namespace {

enum class DistillationParseResult {
  kSuccess = 0,
  kParseFailure = 1,
  kNoResult = 2,
};

// This is a minimal concrete class that inherits from DistillerPage. It
// overrides the implementation of distillation to immediately call the
// completion handler, simulating a success or failure result for the test.
class TestDistillerPage : public DistillerPage {
 public:
  // This enum controls which distillation outcome the mock will simulate.
  enum class SimulatedResult { kSuccess, kParseFailure, kNoResult };

  TestDistillerPage() = default;

  // Configures the mock to simulate a specific result.
  void SetNextResult(SimulatedResult result) { simulate_result_ = result; }

  bool ShouldFetchOfflineData() override { return false; }

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
    }
  }

 private:
  SimulatedResult simulate_result_ = SimulatedResult::kSuccess;
};

class DistillerPageTest : public testing::Test {
 protected:
  DistillerPageTest() = default;

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

}  // namespace

}  // namespace dom_distiller
