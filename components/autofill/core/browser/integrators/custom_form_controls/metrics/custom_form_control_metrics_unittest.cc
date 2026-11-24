// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/custom_form_controls/metrics/custom_form_control_metrics.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::autofill_metrics {
namespace {

class CustomFormControlMetricsTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

// Tests that the script load logging callback logs the time since the callback
// was created if the script was loaded.
TEST_F(CustomFormControlMetricsTest, ScriptLoadLoggingCallback_Success) {
  base::HistogramTester histogram_tester;
  base::OnceCallback<void(bool)> callback =
      CreateCustomFormControlScriptLoadLoggingCallback();
  task_environment_.FastForwardBy(base::Milliseconds(30));
  std::move(callback).Run(true);
  histogram_tester.ExpectUniqueTimeSample(
      "Autofill.CustomFormControls.ScriptLoadLatency", base::Milliseconds(30),
      1);
}

// Tests that the script load logging callback logs no latency if the script
// wasn't loaded.
TEST_F(CustomFormControlMetricsTest, ScriptLoadLoggingCallback_Failure) {
  base::HistogramTester histogram_tester;
  CreateCustomFormControlScriptLoadLoggingCallback().Run(false);
  histogram_tester.ExpectTotalCount(
      "Autofill.CustomFormControls.ScriptLoadLatency", 0);
}

}  // namespace
}  // namespace autofill::autofill_metrics
