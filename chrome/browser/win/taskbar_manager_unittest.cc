// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/taskbar_manager.h"

#include <shlobj.h>

#include <algorithm>
#include <atomic>
#include <string>

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions_win.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/win/scoped_co_mem.h"
#include "chrome/installer/util/shell_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

// Because accessing limited access features requires adding a resource
// to the .rc file, and tests don't have .rc files, all we can test
// is that requesting taskbar operations calls the callback with the
// kFeatureNotAvailable failure result.

static constexpr char kShouldPinResultMetric[] =
    "Windows.ShouldPinToTaskbarResult";
static constexpr char kInfobarShouldPinResultMetric[] =
    "Windows.ShouldPinToTaskbarResult.PinToTaskbarInfoBar";
static constexpr char kSettingsShouldPinResultMetric[] =
    "Windows.ShouldPinToTaskbarResult.SettingsPage";
static constexpr char kPinResultMetric[] = "Windows.TaskbarPinResult";
static constexpr char kInfobarPinResultMetric[] =
    "Windows.TaskbarPinResult.PinToTaskbarInfoBar";
static constexpr char kSettingsPinResultMetric[] =
    "Windows.TaskbarPinResult.SettingsPage";

namespace {

// Number of pin flows the concurrency tests request at the same time.
constexpr int kConcurrentFlowCount = 8;

std::wstring AppUserModelIdForFlow(int index) {
  return L"Chromium.TaskbarManagerTest." + base::NumberToWString(index);
}

// Returns the process-wide App User Model ID, or an empty string if none is
// set.
std::wstring GetProcessAppUserModelId() {
  base::win::ScopedCoMem<wchar_t> app_user_model_id;
  if (FAILED(::GetCurrentProcessExplicitAppUserModelID(&app_user_model_id))) {
    return std::wstring();
  }
  return std::wstring(app_user_model_id.get());
}

}  // namespace

class TaskbarManagerTest : public testing::Test {
 public:
  void OnCanPinToTaskbarResult(bool result) {
    result_ = result;
    got_result_.Quit();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  bool result_ = false;
  base::RunLoop got_result_;
  base::HistogramTester histogram_tester_;
};

TEST_F(TaskbarManagerTest, ShouldOfferToPin) {
  browser_util::ShouldOfferToPin(
      ShellUtil::GetBrowserModelId(/*is_per_user_install=*/true),
      browser_util::PinAppToTaskbarChannel::kPinToTaskbarInfoBar,
      base::BindOnce(&TaskbarManagerTest::OnCanPinToTaskbarResult,
                     base::Unretained(this)));

  got_result_.Run();
  EXPECT_FALSE(result_);
  int expected_bucket_count =
      browser_util::PinLimitedAccessFeatureAvailable() ? 0 : 1;
  histogram_tester_.ExpectBucketCount(
      kShouldPinResultMetric,
      browser_util::PinResultMetric::kFeatureNotAvailable,
      expected_bucket_count);
  histogram_tester_.ExpectBucketCount(
      kInfobarShouldPinResultMetric,
      browser_util::PinResultMetric::kFeatureNotAvailable,
      expected_bucket_count);
  histogram_tester_.ExpectBucketCount(
      kSettingsShouldPinResultMetric,
      browser_util::PinResultMetric::kFeatureNotAvailable, 0);
}

TEST_F(TaskbarManagerTest, PinToTaskbar) {
  browser_util::PinAppToTaskbar(
      ShellUtil::GetBrowserModelId(/*is_per_user_install=*/true),
      browser_util::PinAppToTaskbarChannel::kSettingsPage,
      base::BindOnce(&TaskbarManagerTest::OnCanPinToTaskbarResult,
                     base::Unretained(this)));

  got_result_.Run();
  int expected_bucket_count =
      browser_util::PinLimitedAccessFeatureAvailable() ? 0 : 1;
  histogram_tester_.ExpectBucketCount(
      kPinResultMetric, browser_util::PinResultMetric::kFeatureNotAvailable,
      expected_bucket_count);
  histogram_tester_.ExpectBucketCount(
      kInfobarPinResultMetric,
      browser_util::PinResultMetric::kFeatureNotAvailable, 0);
  histogram_tester_.ExpectBucketCount(
      kSettingsPinResultMetric,
      browser_util::PinResultMetric::kFeatureNotAvailable,
      expected_bucket_count);
  EXPECT_FALSE(result_);
}

// Most of a pin flow now runs on a background sequence, and a flow that bails
// out early (as it does here, because the limited access feature is not
// available in tests) produces its result there. Callers such as the default
// browser infobar touch UI from the result callback, so it must still be
// delivered on the sequence that requested the pin.
TEST_F(TaskbarManagerTest, ResultCallbackRunsOnCallingUIThread) {
  bool ran_on_ui_thread = false;

  browser_util::ShouldOfferToPin(
      ShellUtil::GetBrowserModelId(/*is_per_user_install=*/true),
      browser_util::PinAppToTaskbarChannel::kPinToTaskbarInfoBar,
      base::BindLambdaForTesting([&](bool) {
        ran_on_ui_thread =
            content::BrowserThread::CurrentlyOn(content::BrowserThread::UI);
        got_result_.Quit();
      }));

  got_result_.Run();
  EXPECT_TRUE(ran_on_ui_thread);
}

// The same contract holds for callers that are not on the UI thread: the result
// goes back to the calling sequence, not to the pin sequence or the UI thread.
TEST_F(TaskbarManagerTest, ResultCallbackRunsOnCallingBackgroundSequence) {
  auto calling_task_runner = base::ThreadPool::CreateSequencedTaskRunner({});
  bool ran_on_calling_sequence = false;

  calling_task_runner->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&, calling_task_runner,
                                             quit = got_result_.QuitClosure()] {
        browser_util::ShouldOfferToPin(
            ShellUtil::GetBrowserModelId(/*is_per_user_install=*/true),
            browser_util::PinAppToTaskbarChannel::kPinToTaskbarInfoBar,
            base::BindLambdaForTesting([&, calling_task_runner, quit](bool) {
              ran_on_calling_sequence =
                  calling_task_runner->RunsTasksInCurrentSequence();
              quit.Run();
            }));
      }));

  got_result_.Run();
  EXPECT_TRUE(ran_on_calling_sequence);
}

// Tests for how taskbar pinning handles the process-wide App User Model ID
// (AUMI). `::SetCurrentProcessExplicitAppUserModelID()` replaces a string owned
// by the shell without any internal synchronization, so overlapping calls from
// two threads corrupt the process heap. Pin flows must therefore be serialized,
// and a flow must never observe another flow's AUMI.
class TaskbarManagerAppUserModelIdTest : public testing::Test {
 public:
  void TearDown() override { ::SetCurrentProcessExplicitAppUserModelID(L""); }

  // Requests a pin flow that behaves like a real one: it holds the process AUMI
  // across a hop to the UI thread and back before releasing it.
  void RequestFlow(int index) {
    browser_util::internal::RunWithProcessAppUserModelId(
        AppUserModelIdForFlow(index),
        base::BindOnce(&TaskbarManagerAppUserModelIdTest::OnFlowStarted,
                       base::Unretained(this), index));
  }

  // Runs on the pin sequence once flow `index` owns the process AUMI.
  void OnFlowStarted(int index, base::ScopedClosureRunner release) {
    running_flows_.fetch_add(1);
    RecordObservation(index);

    // Keep owning the AUMI across a round trip to the UI thread, the way the
    // real flow does while `RequestPinCurrentAppAsync()` is pending. The reply
    // runs back on the pin sequence.
    content::GetUIThreadTaskRunner({})->PostTaskAndReply(
        FROM_HERE, base::DoNothing(),
        base::BindOnce(&TaskbarManagerAppUserModelIdTest::OnFlowFinished,
                       base::Unretained(this), index, std::move(release)));
  }

  void OnFlowFinished(int index, base::ScopedClosureRunner release) {
    RecordObservation(index);
    running_flows_.fetch_sub(1);

    // Hand the AUMI to the next queued flow. The release hops back to this
    // sequence, so the task posted below is ordered after it and only reports
    // the flow as done once the AUMI has actually been handed over.
    release.RunAndReset();
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&TaskbarManagerAppUserModelIdTest::OnFlowReleased,
                       base::Unretained(this)));
  }

  void OnFlowReleased() {
    finished_flows_.fetch_add(1);
    flow_done_.Run();
  }

  // Records what flow `index` sees while it is supposed to exclusively own the
  // process AUMI.
  void RecordObservation(int index) {
    max_running_flows_.store(
        std::max(max_running_flows_.load(), running_flows_.load()));
    if (GetProcessAppUserModelId() != AppUserModelIdForFlow(index)) {
      unexpected_app_user_model_ids_.fetch_add(1);
    }
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;

  // Run on the pin sequence once a flow has released the AUMI.
  base::RepeatingClosure flow_done_;

  std::atomic<int> running_flows_{0};
  std::atomic<int> max_running_flows_{0};
  std::atomic<int> finished_flows_{0};
  std::atomic<int> unexpected_app_user_model_ids_{0};
};

// Regression test for the heap corruption that caused the original fix for
// crbug.com/522868206 to be reverted: pin flows used to update the process AUMI
// from whichever thread they happened to be running on, so two overlapping
// flows could call `::SetCurrentProcessExplicitAppUserModelID()` concurrently.
// Requesting many flows at once from several threads must serialize them.
TEST_F(TaskbarManagerAppUserModelIdTest, ConcurrentFlowsNeverOverlap) {
  base::RunLoop run_loop;
  flow_done_ =
      base::BarrierClosure(kConcurrentFlowCount, run_loop.QuitClosure());

  // Request the flows from the thread pool so that they really do race with
  // each other and with the pin sequence.
  for (int i = 0; i < kConcurrentFlowCount; ++i) {
    base::ThreadPool::PostTask(
        FROM_HERE,
        base::BindOnce(&TaskbarManagerAppUserModelIdTest::RequestFlow,
                       base::Unretained(this), i));
  }

  run_loop.Run();

  EXPECT_EQ(kConcurrentFlowCount, finished_flows_.load());
  // Two flows owning the AUMI at once is what corrupts the heap.
  EXPECT_EQ(1, max_running_flows_.load());
  // A flow must never see an AUMI set by another flow.
  EXPECT_EQ(0, unexpected_app_user_model_ids_.load());
  // The AUMI is restored once every flow is done.
  EXPECT_TRUE(GetProcessAppUserModelId().empty());
}

// The same, but with all the flows requested from the pin sequence itself,
// which exercises the queueing path directly.
TEST_F(TaskbarManagerAppUserModelIdTest, QueuedFlowsRunOneAtATime) {
  base::RunLoop run_loop;
  flow_done_ =
      base::BarrierClosure(kConcurrentFlowCount, run_loop.QuitClosure());

  for (int i = 0; i < kConcurrentFlowCount; ++i) {
    RequestFlow(i);
  }

  run_loop.Run();

  EXPECT_EQ(kConcurrentFlowCount, finished_flows_.load());
  EXPECT_EQ(1, max_running_flows_.load());
  EXPECT_EQ(0, unexpected_app_user_model_ids_.load());
  EXPECT_TRUE(GetProcessAppUserModelId().empty());
}

// A flow that drops its release runner without doing anything must still clear
// the process AUMI and hand it to the next flow, rather than stalling every
// subsequent pin request. The second flow only runs at all if the first flow's
// dropped runner released the AUMI.
TEST_F(TaskbarManagerAppUserModelIdTest, DroppedFlowReleasesAppUserModelId) {
  base::RunLoop run_loop;
  base::RepeatingClosure quit = run_loop.QuitClosure();
  std::wstring first_observed;
  std::wstring second_observed;

  browser_util::internal::RunWithProcessAppUserModelId(
      AppUserModelIdForFlow(0),
      base::BindLambdaForTesting([&](base::ScopedClosureRunner release) {
        first_observed = GetProcessAppUserModelId();
        // `release` is dropped as soon as this returns.
      }));
  browser_util::internal::RunWithProcessAppUserModelId(
      AppUserModelIdForFlow(1),
      base::BindLambdaForTesting([&](base::ScopedClosureRunner release) {
        second_observed = GetProcessAppUserModelId();
        // Release explicitly and quit from a task ordered after the release, so
        // that the AUMI is known to be cleared once `Run()` returns.
        release.RunAndReset();
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                                 quit);
      }));

  run_loop.Run();

  EXPECT_EQ(AppUserModelIdForFlow(0), first_observed);
  EXPECT_EQ(AppUserModelIdForFlow(1), second_observed);
  EXPECT_TRUE(GetProcessAppUserModelId().empty());
}

// The public entry points must tolerate overlapping requests: every request has
// to complete. `ShouldOfferToPin()` is used rather than `PinAppToTaskbar()`
// because it never reaches `RequestPinCurrentAppAsync()`, which would show a
// dialog on machines where the limited access feature is unlocked.
TEST_F(TaskbarManagerAppUserModelIdTest, ConcurrentPublicRequestsAllComplete) {
  base::RunLoop run_loop;
  base::RepeatingClosure got_result =
      base::BarrierClosure(kConcurrentFlowCount, run_loop.QuitClosure());
  int results = 0;
  auto count_result = base::BindLambdaForTesting([&](bool) {
    ++results;
    got_result.Run();
  });

  for (int i = 0; i < kConcurrentFlowCount; ++i) {
    browser_util::ShouldOfferToPin(
        ShellUtil::GetBrowserModelId(/*is_per_user_install=*/true),
        browser_util::PinAppToTaskbarChannel::kPinToTaskbarInfoBar,
        count_result);
  }

  run_loop.Run();

  EXPECT_EQ(kConcurrentFlowCount, results);
}
