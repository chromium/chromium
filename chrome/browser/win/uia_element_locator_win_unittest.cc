// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/uia_element_locator_win.h"

#include <windows.h>

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/multiprocess_test.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/win/message_window.h"
#include "chrome/services/util_win/public/mojom/uia_element_probe.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"
#include "ui/gfx/geometry/rect.h"

namespace {

using FailureReason = UiaElementLocatorWin::FailureReason;
using Measurement = UiaElementLocatorWin::Measurement;
using Result = UiaElementLocatorWin::Result;
using LocateFuture = base::test::TestFuture<Result>;

constexpr gfx::Rect kWindowBounds(100, 200, 300, 400);
constexpr gfx::Rect kElementBounds(120, 300, 200, 40);
constexpr gfx::Rect kLowerElementBounds(120, 500, 200, 40);

// All Locate()'s own check needs: enough to read the pid.
constexpr DWORD kProcessAccess = PROCESS_QUERY_LIMITED_INFORMATION;

base::Process OpenWithExpectedAccess(base::ProcessId pid) {
  return base::Process::OpenWithAccess(pid, kProcessAccess);
}

base::Process OpenCurrentProcess() {
  return OpenWithExpectedAccess(::GetCurrentProcessId());
}

// Terminated by the test.
MULTIPROCESS_TEST_MAIN(UiaElementLocatorLiveProcess) {
  base::PlatformThread::Sleep(TestTimeouts::action_max_timeout());
  return 0;
}

MULTIPROCESS_TEST_MAIN(UiaElementLocatorExitingProcess) {
  return 0;
}

Measurement MakeMeasurement(const gfx::Rect& element_bounds,
                            const gfx::Rect& window_bounds) {
  return {element_bounds, window_bounds};
}

// Answers from what is staged, or holds replies (a suspended target). Held
// replies are run, never dropped, since a dropped Mojo reply breaks the pipe.
class FakeProbe : public chrome::mojom::UiaElementProbe {
 public:
  FakeProbe() = default;
  ~FakeProbe() override { DrainHeld(); }

  void Bind(mojo::PendingReceiver<chrome::mojom::UiaElementProbe> pending) {
    DrainHeld();
    receiver_.reset();
    receiver_.Bind(std::move(pending));
  }

  void SetMeasurement(const gfx::Rect& element_bounds,
                      const gfx::Rect& window_bounds) {
    measurement_ = MakeMeasurement(element_bounds, window_bounds);
  }

  void SetReturnsNothing() { measurement_.reset(); }

  void set_holds_replies(bool holds) { holds_replies_ = holds; }
  bool holds_replies() const { return holds_replies_; }

  // Closes the pipe first: dropping a reply while it is still open is an error.
  void Disconnect() {
    receiver_.reset();
    held_.clear();
  }

  void RespondToHeld(size_t index,
                     const gfx::Rect& element_bounds,
                     const gfx::Rect& window_bounds) {
    ASSERT_LT(index, held_.size());
    ASSERT_TRUE(held_[index]);
    std::move(held_[index]).Run(MakeMeasurement(element_bounds, window_bounds));
  }

  // Returns once FindElement() has been called `count` times in total.
  void WaitForCalls(int count) {
    while (call_count_ < count) {
      base::RunLoop run_loop;
      on_call_ = run_loop.QuitClosure();
      run_loop.Run();
    }
  }

  size_t held_count() const { return held_.size(); }
  int call_count() const { return call_count_; }

  const UiaElementLocatorWin::Target& last_window() const {
    return last_window_;
  }
  const UiaElementLocatorWin::Query& last_query() const { return last_query_; }

  // chrome::mojom::UiaElementProbe:
  void FindElement(UiaElementLocatorWin::Target window,
                   const UiaElementLocatorWin::Query& query,
                   FindElementCallback callback) override {
    ++call_count_;
    last_window_ = std::move(window);
    last_query_ = query;
    if (on_call_) {
      std::move(on_call_).Run();
    }
    if (holds_replies_) {
      held_.push_back(std::move(callback));
      return;
    }
    std::move(callback).Run(measurement_);
  }

 private:
  void DrainHeld() {
    for (auto& callback : held_) {
      if (callback) {
        std::move(callback).Run(std::nullopt);
      }
    }
    held_.clear();
  }

  mojo::Receiver<chrome::mojom::UiaElementProbe> receiver_{this};
  std::optional<Measurement> measurement_;
  std::vector<FindElementCallback> held_;
  bool holds_replies_ = false;
  int call_count_ = 0;
  UiaElementLocatorWin::Target last_window_;
  UiaElementLocatorWin::Query last_query_;
  base::OnceClosure on_call_;
};

class TestLocator : public UiaElementLocatorWin {
 public:
  explicit TestLocator(FakeProbe* fake) : fake_(fake) {}

  int launch_count() const { return launch_count_; }

  // Makes LaunchService() come back empty-handed, as when the service process
  // cannot be started.
  void set_launch_fails(bool fails) { launch_fails_ = fails; }

 protected:
  mojo::Remote<chrome::mojom::UiaElementProbe> LaunchService() override {
    ++launch_count_;
    mojo::Remote<chrome::mojom::UiaElementProbe> remote;
    if (!launch_fails_) {
      fake_->Bind(remote.BindNewPipeAndPassReceiver());
    }
    return remote;
  }

 private:
  raw_ptr<FakeProbe> fake_;
  int launch_count_ = 0;
  bool launch_fails_ = false;
};

class UiaElementLocatorWinTest : public base::MultiProcessTest {
 protected:
  void SetUp() override {
    ASSERT_TRUE(window_.Create(base::BindRepeating(
        [](UINT, WPARAM, LPARAM, LRESULT*) { return false; })));
  }

  UiaElementLocatorWin::Target target() const {
    return {window_.hwnd(), OpenCurrentProcess(), L"TargetClass"};
  }

  static UiaElementLocatorWin::Query query() {
    return {L"TheId", L"theid", 50000};
  }

  // Returns once the request reached the fake and, unless the fake holds its
  // replies, once the answer is back.
  void Locate(UiaElementLocatorWin& locator, LocateFuture& future) {
    const int calls = fake_.call_count();
    locator.Locate(target(), query(), future.GetCallback());
    fake_.WaitForCalls(calls + 1);
    if (!fake_.holds_replies()) {
      EXPECT_TRUE(future.Wait());
    }
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME,
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI};
  base::win::MessageWindow window_;
  FakeProbe fake_;
};

TEST_F(UiaElementLocatorWinTest, LocatesAnElement) {
  fake_.SetMeasurement(kElementBounds, kWindowBounds);
  TestLocator locator(&fake_);

  LocateFuture future;
  Locate(locator, future);

  ASSERT_TRUE(future.Wait());
  ASSERT_THAT(future.Get(), base::test::HasValue());
  EXPECT_EQ(future.Get()->element_bounds, kElementBounds);
  EXPECT_EQ(future.Get()->window_bounds, kWindowBounds);
}

TEST_F(UiaElementLocatorWinTest, PassesTargetAndQueryThrough) {
  fake_.SetMeasurement(kElementBounds, kWindowBounds);
  TestLocator locator(&fake_);

  LocateFuture future;
  Locate(locator, future);

  EXPECT_EQ(fake_.last_window().hwnd, window_.hwnd());
  ASSERT_TRUE(fake_.last_window().process.IsValid());
  EXPECT_EQ(fake_.last_window().process.Pid(), ::GetCurrentProcessId());
  EXPECT_EQ(fake_.last_window().window_class, L"TargetClass");
  EXPECT_EQ(fake_.last_query().automation_id, L"TheId");
  EXPECT_EQ(fake_.last_query().fallback_automation_id_fragment, L"theid");
  EXPECT_EQ(fake_.last_query().fallback_control_type, 50000);
  EXPECT_EQ(fake_.last_query().wait_for, base::TimeDelta());
}

TEST_F(UiaElementLocatorWinTest, PassesTheWaitThrough) {
  fake_.SetMeasurement(kElementBounds, kWindowBounds);
  TestLocator locator(&fake_);

  UiaElementLocatorWin::Query waiting_query = query();
  waiting_query.wait_for = base::Seconds(7);
  LocateFuture future;
  locator.Locate(target(), waiting_query, future.GetCallback());
  ASSERT_TRUE(future.Wait());

  EXPECT_EQ(fake_.last_query().wait_for, base::Seconds(7));
}

// The service is allowed to take as long as it was told to wait, so the
// deadline is that plus the usual margin, not the margin alone.
TEST_F(UiaElementLocatorWinTest, DeadlineIncludesTheWait) {
  fake_.set_holds_replies(true);
  TestLocator locator(&fake_);

  UiaElementLocatorWin::Query waiting_query = query();
  waiting_query.wait_for = base::Seconds(7);
  LocateFuture future;
  locator.Locate(target(), waiting_query, future.GetCallback());
  fake_.WaitForCalls(1);

  task_environment_.FastForwardBy(UiaElementLocatorWin::kLocateTimeout);
  EXPECT_FALSE(future.IsReady());

  task_environment_.FastForwardBy(base::Seconds(7));
  ASSERT_TRUE(future.Wait());
  EXPECT_THAT(future.Get(), base::test::ErrorIs(FailureReason::kTimedOut));
}

TEST_F(UiaElementLocatorWinTest, NotFound) {
  fake_.SetReturnsNothing();
  TestLocator locator(&fake_);

  LocateFuture future;
  Locate(locator, future);

  EXPECT_THAT(future.Get(), base::test::ErrorIs(FailureReason::kNotFound));
}

TEST_F(UiaElementLocatorWinTest, ElementOutsideTheWindowIsUnusable) {
  fake_.SetMeasurement(gfx::Rect(120, 5000, 200, 40), kWindowBounds);
  TestLocator locator(&fake_);

  LocateFuture future;
  Locate(locator, future);

  EXPECT_THAT(future.Get(), base::test::ErrorIs(FailureReason::kUnusable));
}

TEST_F(UiaElementLocatorWinTest, GoneWindowAnswersWithoutLaunching) {
  TestLocator locator(&fake_);

  LocateFuture future;
  locator.Locate({nullptr, OpenCurrentProcess(), L"TargetClass"}, query(),
                 future.GetCallback());

  // Answered without the service, but like every answer, not synchronously.
  EXPECT_FALSE(future.IsReady());
  ASSERT_TRUE(future.Wait());
  EXPECT_THAT(future.Get(), base::test::ErrorIs(FailureReason::kWindowGone));
  EXPECT_EQ(locator.launch_count(), 0);
}

TEST_F(UiaElementLocatorWinTest, DestroyedWindowAnswersWithoutLaunching) {
  TestLocator locator(&fake_);

  HWND hwnd;
  {
    base::win::MessageWindow gone;
    ASSERT_TRUE(gone.Create(base::BindRepeating(
        [](UINT, WPARAM, LPARAM, LRESULT*) { return false; })));
    hwnd = gone.hwnd();
  }
  ASSERT_FALSE(::IsWindow(hwnd));

  LocateFuture future;
  locator.Locate({hwnd, OpenCurrentProcess(), L"TargetClass"}, query(),
                 future.GetCallback());

  ASSERT_TRUE(future.Wait());
  EXPECT_THAT(future.Get(), base::test::ErrorIs(FailureReason::kWindowGone));
  EXPECT_EQ(locator.launch_count(), 0);
}

TEST_F(UiaElementLocatorWinTest, UnidentifiedProcessAnswersWithoutLaunching) {
  TestLocator locator(&fake_);

  LocateFuture future;
  locator.Locate({window_.hwnd(), base::Process(), L"TargetClass"}, query(),
                 future.GetCallback());

  ASSERT_TRUE(future.Wait());
  EXPECT_THAT(future.Get(), base::test::ErrorIs(FailureReason::kWindowGone));
  EXPECT_EQ(locator.launch_count(), 0);
}

TEST_F(UiaElementLocatorWinTest, ExitedProcessAnswersWithoutLaunching) {
  base::Process child = SpawnChild("UiaElementLocatorExitingProcess");
  ASSERT_TRUE(child.IsValid());
  int exit_code = -1;
  ASSERT_TRUE(child.WaitForExit(&exit_code));
  TestLocator locator(&fake_);

  LocateFuture future;
  locator.Locate({window_.hwnd(), std::move(child), L"TargetClass"}, query(),
                 future.GetCallback());

  ASSERT_TRUE(future.Wait());
  EXPECT_THAT(future.Get(), base::test::ErrorIs(FailureReason::kWindowGone));
  EXPECT_EQ(locator.launch_count(), 0);
}

TEST_F(UiaElementLocatorWinTest,
       WindowOfAnotherProcessAnswersWithoutLaunching) {
  base::Process child = SpawnChild("UiaElementLocatorLiveProcess");
  ASSERT_TRUE(child.IsValid());
  base::ScopedClosureRunner terminate_child(base::BindLambdaForTesting(
      [&child] { child.Terminate(0, /*wait=*/true); }));
  base::Process other = OpenWithExpectedAccess(child.Pid());
  ASSERT_TRUE(other.IsValid());
  TestLocator locator(&fake_);

  LocateFuture future;
  locator.Locate({window_.hwnd(), std::move(other), L"TargetClass"}, query(),
                 future.GetCallback());

  ASSERT_TRUE(future.Wait());
  EXPECT_THAT(future.Get(), base::test::ErrorIs(FailureReason::kWindowGone));
  EXPECT_EQ(locator.launch_count(), 0);
}

TEST_F(UiaElementLocatorWinTest, TimesOut) {
  fake_.set_holds_replies(true);
  TestLocator locator(&fake_);

  LocateFuture future;
  Locate(locator, future);
  EXPECT_FALSE(future.IsReady());

  task_environment_.FastForwardBy(UiaElementLocatorWin::kLocateTimeout);

  ASSERT_TRUE(future.Wait());
  EXPECT_THAT(future.Get(), base::test::ErrorIs(FailureReason::kTimedOut));
}

TEST_F(UiaElementLocatorWinTest, DisconnectInFlight) {
  fake_.set_holds_replies(true);
  TestLocator locator(&fake_);

  LocateFuture future;
  Locate(locator, future);
  ASSERT_FALSE(future.IsReady());

  fake_.Disconnect();
  ASSERT_TRUE(future.Wait());

  EXPECT_THAT(future.Get(), base::test::ErrorIs(FailureReason::kDisconnected));
}

TEST_F(UiaElementLocatorWinTest, DisconnectWhileIdleDoesNotTaintTheNext) {
  fake_.SetMeasurement(kElementBounds, kWindowBounds);
  TestLocator locator(&fake_);

  LocateFuture first;
  Locate(locator, first);
  ASSERT_THAT(first.Get(), base::test::HasValue());

  base::test::TestFuture<void> disconnected;
  locator.SetOnDisconnectedForTesting(disconnected.GetCallback());
  fake_.Disconnect();
  ASSERT_TRUE(disconnected.Wait());

  LocateFuture second;
  Locate(locator, second);

  EXPECT_THAT(second.Get(), base::test::HasValue());
  EXPECT_EQ(locator.launch_count(), 2);
}

// The service may die with its disconnect not yet observed when the next
// request goes out. That request must still be answered as a disconnect, not
// as a reply the service never gave.
TEST_F(UiaElementLocatorWinTest, RequestOnAPipeAlreadyDeadIsDisconnected) {
  fake_.SetMeasurement(kElementBounds, kWindowBounds);
  TestLocator locator(&fake_);

  LocateFuture first;
  Locate(locator, first);
  ASSERT_THAT(first.Get(), base::test::HasValue());

  // Nothing runs between the drop and the request.
  fake_.Disconnect();
  LocateFuture second;
  locator.Locate(target(), query(), second.GetCallback());
  ASSERT_TRUE(second.Wait());

  EXPECT_THAT(second.Get(), base::test::ErrorIs(FailureReason::kDisconnected));
  EXPECT_EQ(fake_.call_count(), 1);

  LocateFuture third;
  Locate(locator, third);

  EXPECT_THAT(third.Get(), base::test::HasValue());
  EXPECT_EQ(locator.launch_count(), 2);
}

TEST_F(UiaElementLocatorWinTest, FailedLaunchAnswersDisconnected) {
  TestLocator locator(&fake_);
  locator.set_launch_fails(true);

  LocateFuture first;
  locator.Locate(target(), query(), first.GetCallback());

  ASSERT_TRUE(first.Wait());
  EXPECT_THAT(first.Get(), base::test::ErrorIs(FailureReason::kDisconnected));
  EXPECT_EQ(locator.launch_count(), 1);
  EXPECT_EQ(fake_.call_count(), 0);

  // The next request tries again.
  locator.set_launch_fails(false);
  fake_.SetMeasurement(kElementBounds, kWindowBounds);
  LocateFuture second;
  Locate(locator, second);

  EXPECT_THAT(second.Get(), base::test::HasValue());
  EXPECT_EQ(locator.launch_count(), 2);
}

TEST_F(UiaElementLocatorWinTest, IdleServiceIsDroppedAndRelaunched) {
  fake_.SetMeasurement(kElementBounds, kWindowBounds);
  TestLocator locator(&fake_);

  LocateFuture first;
  Locate(locator, first);
  ASSERT_THAT(first.Get(), base::test::HasValue());
  ASSERT_EQ(locator.launch_count(), 1);

  task_environment_.FastForwardBy(UiaElementLocatorWin::kIdleTimeout / 2);
  LocateFuture second;
  Locate(locator, second);
  ASSERT_THAT(second.Get(), base::test::HasValue());
  EXPECT_EQ(locator.launch_count(), 1);

  task_environment_.FastForwardBy(UiaElementLocatorWin::kIdleTimeout);
  LocateFuture third;
  Locate(locator, third);

  EXPECT_THAT(third.Get(), base::test::HasValue());
  EXPECT_EQ(locator.launch_count(), 2);
}

// A reply for the layout the window has left must not answer the new caller.
TEST_F(UiaElementLocatorWinTest, NewerRequestSupersedesTheOneInFlight) {
  fake_.set_holds_replies(true);
  TestLocator locator(&fake_);

  LocateFuture first;
  Locate(locator, first);
  ASSERT_FALSE(first.IsReady());
  ASSERT_EQ(fake_.held_count(), 1u);

  LocateFuture second;
  locator.Locate(target(), query(), second.GetCallback());

  // The first is answered from a task, not from within the second's Locate().
  EXPECT_FALSE(first.IsReady());
  ASSERT_TRUE(first.Wait());
  EXPECT_THAT(first.Get(), base::test::ErrorIs(FailureReason::kSuperseded));
  fake_.WaitForCalls(2);
  EXPECT_FALSE(second.IsReady());
  ASSERT_EQ(fake_.held_count(), 2u);

  // Replies come back in order, so answering from the second means the first
  // was already dropped rather than passed on.
  fake_.RespondToHeld(0, kElementBounds, kWindowBounds);
  fake_.RespondToHeld(1, kLowerElementBounds, kWindowBounds);
  ASSERT_TRUE(second.Wait());

  EXPECT_THAT(second.Get(), base::test::HasValue());
  EXPECT_EQ(second.Get()->element_bounds, kLowerElementBounds);
}

// A request answered on the spot still takes over from the one in flight, so
// that one cannot be answered later, after the newer request already was.
TEST_F(UiaElementLocatorWinTest, GoneWindowSupersedesTheRequestInFlight) {
  fake_.set_holds_replies(true);
  TestLocator locator(&fake_);

  LocateFuture first;
  Locate(locator, first);
  ASSERT_FALSE(first.IsReady());

  LocateFuture second;
  locator.Locate({nullptr, OpenCurrentProcess(), L"TargetClass"}, query(),
                 second.GetCallback());

  ASSERT_TRUE(first.Wait());
  EXPECT_THAT(first.Get(), base::test::ErrorIs(FailureReason::kSuperseded));
  ASSERT_TRUE(second.Wait());
  EXPECT_THAT(second.Get(), base::test::ErrorIs(FailureReason::kWindowGone));
  EXPECT_EQ(fake_.call_count(), 1);

  // The late reply reaches no one (a TestFuture fails on a second value), and
  // the first request's deadline no longer fires: it would drop the service.
  fake_.RespondToHeld(0, kElementBounds, kWindowBounds);
  task_environment_.FastForwardBy(UiaElementLocatorWin::kLocateTimeout);
  fake_.set_holds_replies(false);
  fake_.SetMeasurement(kElementBounds, kWindowBounds);
  LocateFuture third;
  Locate(locator, third);

  EXPECT_THAT(third.Get(), base::test::HasValue());
  EXPECT_EQ(locator.launch_count(), 1);
}

TEST_F(UiaElementLocatorWinTest, SupersedingKeepsTheService) {
  fake_.set_holds_replies(true);
  TestLocator locator(&fake_);

  locator.Locate(target(), query(), base::DoNothing());
  locator.Locate(target(), query(), base::DoNothing());
  fake_.WaitForCalls(2);

  EXPECT_EQ(locator.launch_count(), 1);
  EXPECT_EQ(fake_.call_count(), 2);
}

TEST_F(UiaElementLocatorWinTest,
       RequestFromASupersededCallbackSupersedesTheReplacement) {
  fake_.set_holds_replies(true);
  TestLocator locator(&fake_);

  std::optional<Result> first_result;
  LocateFuture third;
  locator.Locate(target(), query(),
                 base::BindLambdaForTesting([&](Result result) {
                   first_result = std::move(result);
                   locator.Locate(target(), query(), third.GetCallback());
                 }));
  fake_.WaitForCalls(1);

  LocateFuture second;
  locator.Locate(target(), query(), second.GetCallback());
  fake_.WaitForCalls(3);

  ASSERT_TRUE(first_result.has_value());
  EXPECT_THAT(*first_result, base::test::ErrorIs(FailureReason::kSuperseded));
  ASSERT_TRUE(second.Wait());
  EXPECT_THAT(second.Get(), base::test::ErrorIs(FailureReason::kSuperseded));
  EXPECT_FALSE(third.IsReady());
  EXPECT_EQ(locator.launch_count(), 1);
  ASSERT_EQ(fake_.held_count(), 3u);

  // Only the last reply may reach a caller.
  fake_.RespondToHeld(0, kElementBounds, kWindowBounds);
  fake_.RespondToHeld(1, kElementBounds, kWindowBounds);
  fake_.RespondToHeld(2, kLowerElementBounds, kWindowBounds);
  ASSERT_TRUE(third.Wait());

  EXPECT_THAT(third.Get(), base::test::HasValue());
  EXPECT_EQ(third.Get()->element_bounds, kLowerElementBounds);
}

TEST_F(UiaElementLocatorWinTest, ServiceIsReused) {
  fake_.SetMeasurement(kElementBounds, kWindowBounds);
  TestLocator locator(&fake_);

  LocateFuture first;
  Locate(locator, first);
  LocateFuture second;
  Locate(locator, second);

  EXPECT_EQ(fake_.call_count(), 2);
  EXPECT_EQ(locator.launch_count(), 1);
}

TEST_F(UiaElementLocatorWinTest, TimingOutDropsTheService) {
  fake_.set_holds_replies(true);
  TestLocator locator(&fake_);

  LocateFuture first;
  Locate(locator, first);
  task_environment_.FastForwardBy(UiaElementLocatorWin::kLocateTimeout);
  ASSERT_THAT(first.Get(), base::test::ErrorIs(FailureReason::kTimedOut));
  ASSERT_EQ(locator.launch_count(), 1);

  fake_.set_holds_replies(false);
  fake_.SetMeasurement(kElementBounds, kWindowBounds);

  LocateFuture second;
  Locate(locator, second);

  EXPECT_THAT(second.Get(), base::test::HasValue());
  EXPECT_EQ(locator.launch_count(), 2);
}

}  // namespace
