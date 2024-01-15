// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/optimization_guide/browser_test_util.h"
#include "chrome/browser/optimization_guide/optimization_guide_internals_ui.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/optimization_guide/core/model_util.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/optimization_guide_internals/webui/url_constants.h"
#include "content/public/test/browser_test.h"

class OptimizationGuideInternalsBrowserTest : public WebUIMochaBrowserTest {
 public:
  ~OptimizationGuideInternalsBrowserTest() override = default;

 protected:
  OptimizationGuideInternalsBrowserTest() {
    set_test_loader_host(
        optimization_guide_internals::kChromeUIOptimizationGuideInternalsHost);
  }

  void RunTestCase(const std::string& testCase) {
    RunTestWithoutTestLoader(
        "optimization_guide_internals/optimization_guide_internals_test.js",
        base::StringPrintf(
            "runMochaTest('OptimizationGuideInternalsTest', '%s');",
            testCase.c_str()));
  }

  base::test::ScopedFeatureList scoped_feature_list_{
      optimization_guide::features::kOptimizationHints};
};

class OptimizationGuideInternalsLoggerBrowserTest
    : public OptimizationGuideInternalsBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    auto* logger = OptimizationGuideKeyedServiceFactory::GetForProfile(
                       browser()->profile())
                       ->GetOptimizationGuideLogger();
    EXPECT_FALSE(logger->ShouldEnableDebugLogs());
    WebUIMochaBrowserTest::SetUpOnMainThread();
  }

  void OnWebContentsAvailable(content::WebContents* web_contents) override {
    // Once the internals page is open, debug logs should get enabled.
    auto* logger = OptimizationGuideKeyedServiceFactory::GetForProfile(
                       browser()->profile())
                       ->GetOptimizationGuideLogger();
    EXPECT_TRUE(logger->ShouldEnableDebugLogs());
  }
};

IN_PROC_BROWSER_TEST_F(OptimizationGuideInternalsLoggerBrowserTest,
                       DebugLogEnabledOnInternalsPage) {
  RunTestCase("EmptyTest");
}

class OptimizationGuideInternalsLogMessageBrowserTest
    : public OptimizationGuideInternalsBrowserTest {
 protected:
  void OnWebContentsAvailable(content::WebContents* web_contents) override {
    auto* service = OptimizationGuideKeyedServiceFactory::GetForProfile(
        browser()->profile());
    service->RegisterOptimizationTypes({optimization_guide::proto::NOSCRIPT});
    chrome::AddTabAt(browser(), GURL(url::kAboutBlankURL), -1, true);

    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("https://foo")));

    service->CanApplyOptimization(GURL("https://foo"),
                                  optimization_guide::proto::NOSCRIPT,
                                  /*optimization_metadata=*/nullptr);
  }
};

// Verifies log message is added when internals page is open.
IN_PROC_BROWSER_TEST_F(OptimizationGuideInternalsLogMessageBrowserTest,
                       InternalsPageOpen) {
  RunTestCase("InternalsPageOpen");
}

class OptimizationGuideInternalsModelsPageBrowserTest
    : public OptimizationGuideInternalsBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    base::FilePath src_dir;
    base::PathService::Get(chrome::DIR_TEST_DATA, &src_dir);

    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        optimization_guide::switches::kModelOverride,
        base::StrCat({
            "OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD",
            optimization_guide::ModelOverrideSeparator(),
            optimization_guide::FilePathToString(
                src_dir.AppendASCII("optimization_guide")
                    .AppendASCII("additional_file_exists.crx3")),
        }));

    WebUIMochaBrowserTest::SetUpOnMainThread();
  }

  void OnWebContentsAvailable(content::WebContents* web_contents) override {
    TriggerModelDownloadForOptimizationTarget(
        optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
  }

 private:
  void TriggerModelDownloadForOptimizationTarget(
      optimization_guide::proto::OptimizationTarget optimization_target) {
    base::RunLoop run_loop;
    optimization_guide::ModelFileObserver model_file_observer;

    model_file_observer.set_model_file_received_callback(
        base::BindLambdaForTesting(
            [&run_loop](optimization_guide::proto::OptimizationTarget
                            optimization_target,
                        base::optional_ref<const optimization_guide::ModelInfo>
                            model_info) {
              base::ScopedAllowBlockingForTesting scoped_allow_blocking;

              EXPECT_TRUE(model_info.has_value());
              EXPECT_EQ(123, model_info->GetVersion());
              EXPECT_TRUE(model_info->GetModelFilePath().IsAbsolute());
              EXPECT_TRUE(base::PathExists(model_info->GetModelFilePath()));

              EXPECT_EQ(1U, model_info->GetAdditionalFiles().size());
              for (const base::FilePath& add_file :
                   model_info->GetAdditionalFiles()) {
                EXPECT_TRUE(add_file.IsAbsolute());
                EXPECT_TRUE(base::PathExists(add_file));
              }

              run_loop.Quit();
            }));

    OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
        ->AddObserverForOptimizationTargetModel(optimization_target,
                                                /*model_metadata=*/std::nullopt,
                                                &model_file_observer);

    run_loop.Run();
  }
};

// Verifies downloaded models are added when #models page is open.
IN_PROC_BROWSER_TEST_F(OptimizationGuideInternalsModelsPageBrowserTest,
                       InternalsModelsPageOpen) {
  RunTestCase("InternalsModelsPageOpen");
}
