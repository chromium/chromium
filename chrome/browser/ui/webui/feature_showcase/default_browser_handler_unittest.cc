// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/feature_showcase/default_browser_handler.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/views/profiles/feature_showcase/feature_showcase_metrics.h"
#include "chrome/browser/ui/webui/intro/intro_ui.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

class FeatureShowcaseDefaultBrowserHandlerTest : public testing::Test {
 public:
  FeatureShowcaseDefaultBrowserHandlerTest() = default;

  void TearDown() override { handler_.reset(); }

  void CreateHandler() {
    handler_remote_.reset();
    handler_ = std::make_unique<DefaultBrowserHandler>(
        handler_remote_.BindNewPipeAndPassReceiver());
  }

  void CreateHandlerForTesting(
      base::OnceClosure on_set_as_default
#if BUILDFLAG(IS_WIN)
      ,
      DefaultBrowserHandler::PinToTaskbarCallbackForTesting on_pin =
          base::NullCallback()
#endif
  ) {
    handler_remote_.reset();
    handler_ = std::make_unique<DefaultBrowserHandler>(
        handler_remote_.BindNewPipeAndPassReceiver(),
        std::move(on_set_as_default)
#if BUILDFLAG(IS_WIN)
            ,
        std::move(on_pin)
#endif
    );
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  mojo::Remote<feature_showcase::mojom::DefaultBrowserPageHandler>
      handler_remote_;
  std::unique_ptr<DefaultBrowserHandler> handler_;
};

TEST_F(FeatureShowcaseDefaultBrowserHandlerTest,
       SetAsDefaultBrowserLogsMetrics) {
  base::HistogramTester histogram_tester;
  CreateHandler();

  handler_remote_->SetAsDefaultBrowser();
  handler_remote_.FlushForTesting();

  histogram_tester.ExpectBucketCount("ProfilePicker.FirstRun.DefaultBrowser",
                                     DefaultBrowserChoice::kClickSetAsDefault,
                                     1);
  histogram_tester.ExpectUniqueSample(
      "ProfilePicker.FREFlow.FeatureShowcase.StepUserAction.DefaultBrowser",
      FeatureShowcaseStepUserAction::kAccepted, 1);
}

TEST_F(FeatureShowcaseDefaultBrowserHandlerTest,
       SkipSetAsDefaultBrowserLogsMetrics) {
  base::HistogramTester histogram_tester;
  CreateHandler();

  handler_remote_->SkipSetAsDefaultBrowser();
  handler_remote_.FlushForTesting();

  histogram_tester.ExpectBucketCount("ProfilePicker.FirstRun.DefaultBrowser",
                                     DefaultBrowserChoice::kSkip, 1);
  histogram_tester.ExpectUniqueSample(
      "ProfilePicker.FREFlow.FeatureShowcase.StepUserAction.DefaultBrowser",
      FeatureShowcaseStepUserAction::kDeclined, 1);
}

TEST_F(FeatureShowcaseDefaultBrowserHandlerTest,
       SetAsDefaultBrowserRunsStartSetAsDefault) {
  base::HistogramTester histogram_tester;
  shell_integration::DefaultBrowserWorker::DisableSetAsDefaultForTesting();

  base::RunLoop run_loop;
  CreateHandlerForTesting(run_loop.QuitClosure());

  handler_remote_->SetAsDefaultBrowser();
  handler_remote_.FlushForTesting();
  run_loop.Run();

  histogram_tester.ExpectTotalCount("DefaultBrowser.SetDefaultResult2", 1);
}

#if BUILDFLAG(IS_WIN)
TEST_F(FeatureShowcaseDefaultBrowserHandlerTest,
       SetAsDefaultBrowserWithCanPin) {
  base::HistogramTester histogram_tester;
  shell_integration::DefaultBrowserWorker::DisableSetAsDefaultForTesting();

  base::RunLoop run_loop;
  CreateHandlerForTesting(base::NullCallback(),
                          base::IgnoreArgs<bool>(run_loop.QuitClosure()));

  handler_->SetCanPin(true);
  handler_remote_->SetAsDefaultBrowser();
  handler_remote_.FlushForTesting();
  run_loop.Run();

  histogram_tester.ExpectTotalCount("Windows.TaskbarPinResult", 1);
}
#endif
