// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/content/content_stability_metrics_provider.h"

#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/metrics/content/extensions_helper.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/hashing.h"
#include "content/public/browser/browser_child_process_observer.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/child_process_termination_info.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_service.mojom.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "sandbox/win/src/sandbox_types.h"
#endif

namespace content {

template <>
sandbox::mojom::Sandbox GetServiceSandboxType<content::mojom::TestService>() {
  // On Windows, the sandbox does not like having a different binary name
  // 'non_existent_path' from the browser process, so set no sandbox here.
#if BUILDFLAG(IS_WIN)
  return sandbox::mojom::Sandbox::kNoSandbox;
#else
  return sandbox::mojom::Sandbox::kService;
#endif
}

}  // namespace content

namespace metrics {

class ContentStabilityProviderBrowserTest
    : public content::ContentBrowserTest,
      content::BrowserChildProcessObserver {
 public:
  ContentStabilityProviderBrowserTest() {
    feature_list_.InitAndDisableFeature(
        features::kSpareRendererForSitePerProcess);
  }

  // Either the process launched, or did not launch. Both cause the run_loop to
  // terminate.
  void BrowserChildProcessLaunchFailed(
      const content::ChildProcessData& data,
      const content::ChildProcessTerminationInfo& info) override {
    if (data.metrics_name == content::mojom::TestService::Name_)
      std::move(done_closure_).Run();
  }

  void BrowserChildProcessLaunchedAndConnected(
      const content::ChildProcessData& data) override {
    if (data.metrics_name == content::mojom::TestService::Name_)
      std::move(done_closure_).Run();
  }

 protected:
  void AddObserver() { content::BrowserChildProcessObserver::Add(this); }

  void RemoveObserver() { content::BrowserChildProcessObserver::Remove(this); }

  base::OnceClosure done_closure_;
  TestingPrefServiceSimple prefs_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ContentStabilityProviderBrowserTest,
                       FailedUtilityProcessLaunches) {
  base::RunLoop run_loop;
  done_closure_ = run_loop.QuitClosure();
  AddObserver();

  ContentStabilityMetricsProvider provider(&prefs_, nullptr);
  base::HistogramTester histogram_tester;

  // Simulate a catastrophic utility process launch failure by specifying a bad
  // path.
  base::CommandLine::ForCurrentProcess()->AppendSwitchPath(
      switches::kBrowserSubprocessPath,
      base::FilePath(FILE_PATH_LITERAL("non_existent_path")));
  mojo::Remote<content::mojom::TestService> test_service;
  content::ServiceProcessHost::Launch(
      test_service.BindNewPipeAndPassReceiver());

  // run_loop runs until either the process launches or fails to launch.
  run_loop.Run();

  RemoveObserver();

  histogram_tester.ExpectUniqueSample(
      "ChildProcess.LaunchFailed.UtilityProcessHash",
      variations::HashName(content::mojom::TestService::Name_), 1);
#if BUILDFLAG(IS_WIN)
  int expected_error_code =
      sandbox::SBOX_ERROR_CANNOT_LAUNCH_UNSANDBOXED_PROCESS;
#else
  int expected_error_code =
      1003;  // content::LaunchResultCode::LAUNCH_RESULT_FAILURE.
#endif
  histogram_tester.ExpectUniqueSample(
      "ChildProcess.LaunchFailed.UtilityProcessErrorCode", expected_error_code,
      1);

#if BUILDFLAG(IS_WIN)
  // Last Error is only recorded on Windows.
  histogram_tester.ExpectUniqueSample("ChildProcess.LaunchFailed.WinLastError",
                                      DWORD{ERROR_FILE_NOT_FOUND}, 1);
#endif
}

// Class to execute a closure after we observer a renderer process launch or
// launch failure.
class RenderProcessCreationObserver
    : content::RenderProcessHostCreationObserver {
 public:
  RenderProcessCreationObserver(base::OnceClosure done_closure)
      : done_closure_(std::move(done_closure)) {}
  ~RenderProcessCreationObserver() override = default;

  void OnRenderProcessHostCreated(
      content::RenderProcessHost* process_host) override {
    if (done_closure_) {
      std::move(done_closure_).Run();
    }
  }

  void OnRenderProcessHostCreationFailed(
      content::RenderProcessHost* host,
      const content::ChildProcessTerminationInfo& info) override {
    ASSERT_EQ(base::TERMINATION_STATUS_LAUNCH_FAILED, info.status);
    if (done_closure_) {
      std::move(done_closure_).Run();
    }
  }

 private:
  base::OnceClosure done_closure_;
};

IN_PROC_BROWSER_TEST_F(ContentStabilityProviderBrowserTest,
                       FailedRendererProcessLaunches) {
  base::RunLoop run_loop;
  ContentStabilityMetricsProvider provider(&prefs_, nullptr);
  base::HistogramTester histogram_tester;
  {
    RenderProcessCreationObserver renderer_observer(run_loop.QuitClosure());

    // Simulate a catastrophic renderer process launch failure by specifying a
    // bad path.
    base::CommandLine::ForCurrentProcess()->AppendSwitchPath(
        switches::kBrowserSubprocessPath,
        base::FilePath(FILE_PATH_LITERAL("non_existent_path")));

    ASSERT_FALSE(content::NavigateToURL(shell(), GURL("about:blank")));

    // run_loop runs until either the process launches or fails to launch.
    run_loop.Run();
  }

  histogram_tester.ExpectUniqueSample(
      "BrowserRenderProcessHost.ChildLaunchFailures",
      1 /* CoarseRendererType::kRenderer */, 1);
#if BUILDFLAG(IS_WIN)
  int expected_error_code = sandbox::SBOX_ERROR_CREATE_PROCESS;
#else
  int expected_error_code =
      1003;  // content::LaunchResultCode::LAUNCH_RESULT_FAILURE.
#endif
  histogram_tester.ExpectUniqueSample(
      "BrowserRenderProcessHost.ChildLaunchFailureCodes", expected_error_code,
      1);

  histogram_tester.ExpectBucketCount(
      "Stability.Counts2", StabilityEventType::kRendererFailedLaunch, 1);
}

}  // namespace metrics
