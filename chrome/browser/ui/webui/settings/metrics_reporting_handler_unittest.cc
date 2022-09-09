// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/branding_buildflags.h"
#include "build/chromeos_buildflags.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS_ASH)

#include "chrome/browser/ui/webui/settings/metrics_reporting_handler.h"

#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace settings {

class TestingMetricsReportingHandler : public MetricsReportingHandler {
 public:
  using MetricsReportingHandler::set_web_ui;
  using MetricsReportingHandler::HandleGetMetricsReporting;
};

class MetricsReportingHandlerTest : public testing::Test {
 public:
  MetricsReportingHandlerTest() {
    // Local state must be set up before |handler_|.
    local_state_ = std::make_unique<ScopedTestingLocalState>(
        TestingBrowserProcess::GetGlobal());

    handler_ = std::make_unique<TestingMetricsReportingHandler>();
    handler_->set_web_ui(&test_web_ui_);
  }

  void SetUp() override {
    ASSERT_EQ(local_state(), g_browser_process->local_state());
    EXPECT_TRUE(test_web_ui()->call_data().empty());

    base::Value::List args;
    args.Append(1);
    handler()->HandleGetMetricsReporting(args);

    EXPECT_TRUE(handler()->IsJavascriptAllowed());
    EXPECT_EQ(1u, test_web_ui()->call_data().size());

    test_web_ui()->ClearTrackedCalls();
  }

  void TearDown() override {
    // For crbug.com/637068 which only run on official bots with no try jobs.
    base::RunLoop().RunUntilIdle();
    handler_.reset();
    base::RunLoop().RunUntilIdle();
    local_state_.reset();
    base::RunLoop().RunUntilIdle();
  }

  PrefService* local_state() { return local_state_->Get(); }
  TestingMetricsReportingHandler* handler() { return handler_.get(); }
  content::TestWebUI* test_web_ui() { return &test_web_ui_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::TestWebUI test_web_ui_;
  std::unique_ptr<ScopedTestingLocalState> local_state_;
  std::unique_ptr<TestingMetricsReportingHandler> handler_;
};

TEST_F(MetricsReportingHandlerTest, PrefChangesNotifyPage) {
  // Toggle the pref.
  local_state()->SetBoolean(
      metrics::prefs::kMetricsReportingEnabled,
      !local_state()->GetBoolean(metrics::prefs::kMetricsReportingEnabled));
  EXPECT_EQ(1u, test_web_ui()->call_data().size());

  test_web_ui()->ClearTrackedCalls();
  handler()->DisallowJavascript();

  // Toggle the pref again, while JavaScript is disabled.
  local_state()->SetBoolean(
      metrics::prefs::kMetricsReportingEnabled,
      !local_state()->GetBoolean(metrics::prefs::kMetricsReportingEnabled));
  EXPECT_TRUE(test_web_ui()->call_data().empty());
}

}  // namespace settings

#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS_ASH)
