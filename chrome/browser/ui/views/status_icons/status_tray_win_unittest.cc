// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/status_icons/status_tray_win.h"

#include <commctrl.h>
#include <shellapi.h>
#include <stddef.h>

#include <atomic>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/test_timeouts.h"
#include "base/thread_annotations.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/win/scoped_gdi_object.h"
#include "chrome/browser/status_icons/status_icon_menu_model.h"
#include "chrome/browser/status_icons/status_icon_observer.h"
#include "chrome/browser/ui/views/status_icons/status_icon_win.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "ui/views/test/views_test_base.h"

namespace {

bool IsValidIcon(HICON icon) {
  ICONINFO info = {};
  if (!GetIconInfo(icon, &info)) {
    return false;
  }
  base::win::ScopedGDIObject<HBITMAP> mask(info.hbmMask);
  base::win::ScopedGDIObject<HBITMAP> color(info.hbmColor);
  return true;
}

// Owns everything accessed by the injected worker callback, including when an
// icon or tray is destroyed while the callback is blocked.
class FakeShell : public base::RefCountedThreadSafe<FakeShell> {
 public:
  FakeShell() = default;
  struct Call {
    DWORD message;
    NOTIFYICONDATA data;
    scoped_refptr<base::SequencedTaskRunner> runner;
    DWORD thread_id;
    bool window_valid;
    bool icons_valid;
  };

  BOOL Notify(DWORD message, const NOTIFYICONDATA& data) {
    bool block = false;
    {
      base::AutoLock lock(lock_);
      if (message_to_block_ == message) {
        message_to_block_.reset();
        block = true;
      }
    }
    if (block) {
      entered_.Signal();
      // A failed assertion must not leave the test process hanging forever.
      base::ScopedAllowBaseSyncPrimitivesForTesting allow_wait;
      EXPECT_TRUE(release_.TimedWait(TestTimeouts::action_max_timeout()));
    }
    Call call{message,
              data,
              base::SequencedTaskRunner::GetCurrentDefault(),
              GetCurrentThreadId(),
              !!IsWindow(data.hWnd),
              (!data.hIcon || IsValidIcon(data.hIcon)) &&
                  (!data.hBalloonIcon || IsValidIcon(data.hBalloonIcon))};
    base::AutoLock lock(lock_);
    calls_.push_back(std::move(call));
    // Wake RunUntil on the UI sequence when a new call is recorded.
    ui_task_runner_->PostTask(FROM_HERE, base::DoNothing());
    return TRUE;
  }

  void Block(DWORD message) {
    base::AutoLock lock(lock_);
    entered_.Reset();
    release_.Reset();
    message_to_block_ = message;
  }
  bool WaitUntilBlocked() {
    base::ScopedAllowBaseSyncPrimitivesForTesting allow_wait;
    return entered_.TimedWait(TestTimeouts::action_timeout());
  }
  void Release() { release_.Signal(); }
  std::vector<Call> calls() {
    base::AutoLock lock(lock_);
    return calls_;
  }
  bool WaitForCalls(size_t count) {
    return base::test::RunUntil([&] { return calls().size() >= count; });
  }

  bool WaitForPendingTasks() {
    if (!WaitForCalls(1)) {
      return false;
    }
    // The marker follows the Shell requests and ForceVisible barrier on the
    // same sequence. Its UI reply follows any earlier visibility reply.
    base::test::TestFuture<void> completed;
    calls().back().runner->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                            completed.GetCallback());
    return completed.Wait();
  }

  void Clear() {
    base::AutoLock lock(lock_);
    calls_.clear();
  }

 private:
  friend class base::RefCountedThreadSafe<FakeShell>;
  ~FakeShell() = default;

  const scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_ =
      base::SingleThreadTaskRunner::GetCurrentDefault();
  base::Lock lock_;
  std::vector<Call> calls_ GUARDED_BY(lock_);
  std::optional<DWORD> message_to_block_ GUARDED_BY(lock_);
  base::WaitableEvent entered_;
  base::WaitableEvent release_;
};

class FakeStatusTrayStateChangerProxy : public StatusTrayStateChangerProxy {
 public:
  FakeStatusTrayStateChangerProxy()
      : enqueue_called_(false), icon_id_(0), window_(NULL) {}

  FakeStatusTrayStateChangerProxy(const FakeStatusTrayStateChangerProxy&) =
      delete;
  FakeStatusTrayStateChangerProxy& operator=(
      const FakeStatusTrayStateChangerProxy&) = delete;

  void EnqueueChange(UINT icon_id, HWND window) override {
    enqueue_called_ = true;
    icon_id_ = icon_id;
    window_ = window;
  }

  bool enqueue_called() { return enqueue_called_; }
  UINT icon_id() { return icon_id_; }
  HWND window() { return window_; }

 private:
  bool enqueue_called_;
  UINT icon_id_;
  HWND window_;
};

class FakeStatusIconObserver : public StatusIconObserver {
 public:
  FakeStatusIconObserver()
      : status_icon_click_count_(0), balloon_clicked_(false) {}
  void OnStatusIconClicked() override { ++status_icon_click_count_; }
  void OnBalloonClicked() override { balloon_clicked_ = true; }
  bool balloon_clicked() const { return balloon_clicked_; }
  size_t status_icon_click_count() const { return status_icon_click_count_; }

 private:
  size_t status_icon_click_count_;
  bool balloon_clicked_;
};

}  // namespace

class StatusTrayWinTest : public ChromeViewsTestBase {
 protected:
  void SetUp() override {
    set_native_widget_type(NativeWidgetType::kDesktop);
    ChromeViewsTestBase::SetUp();
    shell_ = base::MakeRefCounted<FakeShell>();
    StatusIconWin::SetShellNotifyIconForTesting(
        base::BindRepeating(&FakeShell::Notify, shell_));
  }

  void TearDown() override {
    shell_->Release();
    // Finish pending fake Shell calls, including cleanup after failed asserts,
    // before clearing the injected callback and tearing down the UI loop.
    base::ThreadPoolInstance::Get()->FlushForTesting();
    const auto calls = shell_->calls();
    EXPECT_TRUE(base::test::RunUntil([&] {
      for (const auto& call : calls) {
        if (IsWindow(call.data.hWnd)) {
          return false;
        }
      }
      return true;
    }));
    StatusIconWin::SetShellNotifyIconForTesting({});
    ChromeViewsTestBase::TearDown();
  }

  scoped_refptr<FakeShell> shell_;
};

StatusIconWin* CreateStatusIcon(StatusTray& tray) {
  return static_cast<StatusIconWin*>(tray.CreateStatusIcon(
      StatusTray::OTHER_ICON, gfx::test::CreateImageSkia(16, 16),
      std::u16string()));
}

void CreateContextMenu(StatusIconWin& icon) {
  auto menu = std::make_unique<StatusIconMenuModel>(nullptr);
  menu->AddItem(0, u"foo");
  icon.SetContextMenu(std::move(menu));
}

TEST_F(StatusTrayWinTest, CreateTray) {
  // Just tests creation/destruction.
  StatusTrayWin tray;
}

TEST_F(StatusTrayWinTest, CreateIconAndMenu) {
  // Create an icon, set the context menu, then shut down.
  StatusTrayWin tray;
  StatusIconWin* icon = CreateStatusIcon(tray);
  CreateContextMenu(*icon);
}

TEST_F(StatusTrayWinTest, ClickOnIcon) {
  // Create an icon, send a fake click event, make sure observer is called.
  StatusTrayWin tray;

  StatusIconWin* icon = CreateStatusIcon(tray);
  FakeStatusIconObserver observer;
  icon->AddObserver(&observer);
  // Mimic a click.
  tray.WndProc(nullptr, icon->message_id(), icon->icon_id(), WM_LBUTTONDOWN);
  // Mimic a right-click - observer should not be called.
  tray.WndProc(nullptr, icon->message_id(), icon->icon_id(), WM_RBUTTONDOWN);
  EXPECT_EQ(1u, observer.status_icon_click_count());
  icon->RemoveObserver(&observer);
}

TEST_F(StatusTrayWinTest, ContextMenu) {
  // Create icon and context menu.
  StatusTrayWin tray;
  StatusIconWin* icon = CreateStatusIcon(tray);
  CreateContextMenu(*icon);

  // Trigger the context menu.
  tray.WndProc(nullptr, icon->message_id(), icon->icon_id(), WM_RBUTTONDOWN);
}

TEST_F(StatusTrayWinTest, ClickOnBalloon) {
  // Create an icon, send a fake click event, make sure observer is called.
  StatusTrayWin tray;
  StatusIconWin* icon = CreateStatusIcon(tray);
  FakeStatusIconObserver observer;
  icon->AddObserver(&observer);
  // Mimic a click.
  tray.WndProc(nullptr, icon->message_id(), icon->icon_id(), TB_INDETERMINATE);
  EXPECT_TRUE(observer.balloon_clicked());
  icon->RemoveObserver(&observer);
}

TEST_F(StatusTrayWinTest, HandleOldIconId) {
  StatusTrayWin tray;
  StatusIconWin* icon = CreateStatusIcon(tray);
  UINT message_id = icon->message_id();
  UINT icon_id = icon->icon_id();

  tray.RemoveStatusIcon(icon);
  tray.WndProc(nullptr, message_id, icon_id, WM_LBUTTONDOWN);
}

TEST_F(StatusTrayWinTest, EnsureVisibleTest) {
  StatusTrayWin tray;

  FakeStatusTrayStateChangerProxy* proxy =
      new FakeStatusTrayStateChangerProxy();
  tray.SetStatusTrayStateChangerProxyForTest(
      std::unique_ptr<StatusTrayStateChangerProxy>(proxy));

  StatusIconWin* icon = CreateStatusIcon(tray);

  icon->ForceVisible();
  ASSERT_TRUE(base::test::RunUntil([&] { return proxy->enqueue_called(); }));
  // |proxy| is owned by |tray|, and |tray| lives to the end of the scope,
  // so calling methods on |proxy| is safe.
  EXPECT_TRUE(proxy->enqueue_called());
  EXPECT_EQ(proxy->window(), icon->window());
  EXPECT_EQ(proxy->icon_id(), icon->icon_id());
}

TEST_F(StatusTrayWinTest, ResetAndUpdatesRunInOrderOffUI) {
  StatusTrayWin tray;
  StatusIconWin* icon = CreateStatusIcon(tray);
  ASSERT_TRUE(shell_->WaitForCalls(3));
  shell_->Clear();

  icon->ResetIcon();
  icon->SetToolTip(u"updated tooltip");
  ASSERT_TRUE(shell_->WaitForCalls(3));

  const auto calls = shell_->calls();
  ASSERT_EQ(3u, calls.size());
  EXPECT_EQ(NIM_DELETE, calls[0].message);
  EXPECT_EQ(NIM_ADD, calls[1].message);
  EXPECT_EQ(NIM_MODIFY, calls[2].message);
  EXPECT_EQ(calls[0].runner, calls[1].runner);
  EXPECT_EQ(calls[0].runner, calls[2].runner);
  EXPECT_EQ(icon->message_id(), calls[1].data.uCallbackMessage);
  EXPECT_TRUE(calls[1].data.uFlags & NIF_ICON);
  EXPECT_STREQ(L"updated tooltip", calls[2].data.szTip);
  for (const auto& call : calls) {
    EXPECT_NE(GetCurrentThreadId(), call.thread_id);
    EXPECT_TRUE(call.window_valid);
    EXPECT_TRUE(call.icons_valid);
  }
}

TEST_F(StatusTrayWinTest, TaskbarCreatedRestoresAllIcons) {
  StatusTrayWin tray;
  StatusIconWin* first = CreateStatusIcon(tray);
  StatusIconWin* second = CreateStatusIcon(tray);
  ASSERT_TRUE(shell_->WaitForCalls(6));
  shell_->Clear();

  SendMessage(first->window(), RegisterWindowMessage(L"TaskbarCreated"), 0, 0);
  ASSERT_TRUE(shell_->WaitForCalls(4));

  const auto calls = shell_->calls();
  ASSERT_EQ(4u, calls.size());
  EXPECT_EQ(NIM_DELETE, calls[0].message);
  EXPECT_EQ(NIM_ADD, calls[1].message);
  EXPECT_EQ(first->icon_id(), calls[1].data.uID);
  EXPECT_EQ(NIM_DELETE, calls[2].message);
  EXPECT_EQ(NIM_ADD, calls[3].message);
  EXPECT_EQ(second->icon_id(), calls[3].data.uID);
  EXPECT_EQ(calls[0].runner, calls[3].runner);
}

TEST_F(StatusTrayWinTest, BlockedResetDoesNotBlockUIOrTrayDestruction) {
  auto tray = std::make_unique<StatusTrayWin>();
  StatusIconWin* icon = CreateStatusIcon(*tray);
  const HWND window = icon->window();
  const UINT message = icon->message_id();
  const UINT id = icon->icon_id();
  ASSERT_TRUE(shell_->WaitForCalls(3));
  shell_->Clear();
  shell_->Block(NIM_DELETE);
  icon->ResetIcon();
  ASSERT_TRUE(shell_->WaitUntilBlocked());

  bool ui_task_ran = false;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&] { ui_task_ran = true; }));
  ASSERT_TRUE(base::test::RunUntil([&] { return ui_task_ran; }));
  EXPECT_TRUE(ui_task_ran);
  tray.reset();
  EXPECT_TRUE(IsWindow(window));
  // The still-live window must no longer call the destroyed tray.
  SendMessage(window, message, id, WM_LBUTTONDOWN);
  SendMessage(window, RegisterWindowMessage(L"TaskbarCreated"), 0, 0);

  // A new tray must be able to register its window while the old one is alive.
  StatusTrayWin new_tray;
  StatusIconWin* new_icon = CreateStatusIcon(new_tray);
  EXPECT_NE(window, new_icon->window());
  shell_->Release();
  ASSERT_TRUE(base::test::RunUntil([&] { return !IsWindow(window); }));
  ASSERT_TRUE(shell_->WaitForCalls(6));
  EXPECT_FALSE(IsWindow(window));
  for (const auto& call : shell_->calls()) {
    EXPECT_TRUE(call.window_valid);
    EXPECT_TRUE(call.icons_valid);
  }
}

TEST_F(StatusTrayWinTest, QueuedImagesSurviveUpdatesAndIconDestruction) {
  StatusTrayWin tray;
  StatusIconWin* icon = CreateStatusIcon(tray);
  ASSERT_TRUE(shell_->WaitForCalls(3));
  shell_->Clear();
  shell_->Block(NIM_MODIFY);
  icon->SetImage(gfx::test::CreateImageSkia(16, 16));
  ASSERT_TRUE(shell_->WaitUntilBlocked());
  icon->SetImage(gfx::test::CreateImageSkia(32, 32));
  icon->DisplayBalloon(gfx::test::CreateImageSkia(16, 16), u"title", u"body",
                       message_center::NotifierId());
  icon->DisplayBalloon(gfx::test::CreateImageSkia(32, 32), u"next", u"body",
                       message_center::NotifierId());
  tray.RemoveStatusIcon(icon).reset();
  shell_->Release();
  ASSERT_TRUE(shell_->WaitForCalls(5));

  const auto calls = shell_->calls();
  ASSERT_EQ(5u, calls.size());
  EXPECT_EQ(NIM_DELETE, calls.back().message);
  EXPECT_NE(nullptr, calls[0].data.hIcon);
  EXPECT_NE(nullptr, calls[2].data.hBalloonIcon);
  for (const auto& call : calls) {
    EXPECT_TRUE(call.icons_valid);
    EXPECT_TRUE(call.window_valid);
  }
}

TEST_F(StatusTrayWinTest, ReusedIconIdIsDeletedBeforeReplacementIsAdded) {
  StatusTrayWin tray;
  auto* first = tray.CreateStatusIcon(StatusTray::BACKGROUND_MODE_ICON,
                                      gfx::test::CreateImageSkia(16, 16), u"");
  ASSERT_TRUE(shell_->WaitForCalls(3));
  shell_->Clear();
  shell_->Block(NIM_DELETE);
  tray.RemoveStatusIcon(first).reset();
  ASSERT_TRUE(shell_->WaitUntilBlocked());
  tray.CreateStatusIcon(StatusTray::BACKGROUND_MODE_ICON,
                        gfx::test::CreateImageSkia(16, 16), u"");
  shell_->Release();
  ASSERT_TRUE(shell_->WaitForCalls(4));
  const auto calls = shell_->calls();
  ASSERT_EQ(4u, calls.size());
  EXPECT_EQ(NIM_DELETE, calls[0].message);
  EXPECT_EQ(NIM_ADD, calls[1].message);
  EXPECT_EQ(calls[0].data.uID, calls[1].data.uID);
  EXPECT_EQ(calls[0].runner, calls[1].runner);
}

TEST_F(StatusTrayWinTest, ForceVisibleWaitsForRegistration) {
  StatusTrayWin tray;
  auto proxy = std::make_unique<FakeStatusTrayStateChangerProxy>();
  auto* proxy_ptr = proxy.get();
  tray.SetStatusTrayStateChangerProxyForTest(std::move(proxy));
  shell_->Block(NIM_ADD);
  StatusIconWin* icon = CreateStatusIcon(tray);
  ASSERT_TRUE(shell_->WaitUntilBlocked());
  icon->ForceVisible();
  base::test::TestFuture<void> ui_task;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, ui_task.GetCallback());
  ASSERT_TRUE(ui_task.Wait());
  EXPECT_FALSE(proxy_ptr->enqueue_called());
  shell_->Release();
  ASSERT_TRUE(
      base::test::RunUntil([&] { return proxy_ptr->enqueue_called(); }));
  EXPECT_TRUE(proxy_ptr->enqueue_called());
}

TEST_F(StatusTrayWinTest, DestroyedIconCancelsVisibilityReply) {
  StatusTrayWin tray;
  auto proxy = std::make_unique<FakeStatusTrayStateChangerProxy>();
  auto* proxy_ptr = proxy.get();
  tray.SetStatusTrayStateChangerProxyForTest(std::move(proxy));
  shell_->Block(NIM_ADD);
  StatusIconWin* icon = CreateStatusIcon(tray);
  ASSERT_TRUE(shell_->WaitUntilBlocked());
  icon->ForceVisible();
  tray.RemoveStatusIcon(icon).reset();
  shell_->Release();
  ASSERT_TRUE(shell_->WaitForPendingTasks());
  EXPECT_FALSE(proxy_ptr->enqueue_called());
}

// Use a separate pool so the real Shutdown contract can be exercised without
// shutting down the Views fixture's TaskEnvironment-owned pool twice.
TEST(StatusIconWinShutdownTest, DoesNotWaitForBlockedShellCall) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::ThreadingMode::MAIN_THREAD_ONLY);
  base::ThreadPoolInstance::CreateAndStartWithDefaultParams("StatusIconTest");
  auto shell = base::MakeRefCounted<FakeShell>();
  StatusIconWin::SetShellNotifyIconForTesting(
      base::BindRepeating(&FakeShell::Notify, shell));
  shell->Block(NIM_ADD);
  HWND window;
  {
    StatusTrayWin tray;
    window = CreateStatusIcon(tray)->window();
    EXPECT_TRUE(shell->WaitUntilBlocked());
  }
  EXPECT_TRUE(IsWindow(window));

  base::WaitableEvent shutdown_finished;
  std::atomic_bool timed_out = false;
  base::Thread watchdog("StatusIconShutdownWatchdog");
  ASSERT_TRUE(watchdog.Start());
  watchdog.task_runner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&] {
        base::ScopedAllowBaseSyncPrimitivesForTesting allow_wait;
        timed_out =
            !shutdown_finished.TimedWait(TestTimeouts::action_timeout());
        // Release on success as well as failure, so JoinForTesting can clean
        // up.
        shell->Release();
      }));
  base::ThreadPoolInstance::Get()->Shutdown();
  shutdown_finished.Signal();
  watchdog.Stop();
  EXPECT_FALSE(timed_out);
  base::ThreadPoolInstance::Get()->JoinForTesting();
  // Only the in-flight registration runs. Queued updates and removal are
  // skipped once shutdown starts.
  const auto calls = shell->calls();
  EXPECT_EQ(calls.size(), 1u);
  if (!calls.empty()) {
    EXPECT_EQ(calls.front().message, NIM_ADD);
  }
  base::ThreadPoolInstance::Set(nullptr);
  EXPECT_TRUE(base::test::RunUntil([&] { return !IsWindow(window); }));
  EXPECT_FALSE(IsWindow(window));
  StatusIconWin::SetShellNotifyIconForTesting({});
}

TEST_F(StatusTrayWinTest, RemovedIconOutlivingTrayCancelsVisibilityReply) {
  auto tray = std::make_unique<StatusTrayWin>();
  shell_->Block(NIM_ADD);
  StatusIconWin* icon = CreateStatusIcon(*tray);
  const HWND window = icon->window();
  ASSERT_TRUE(shell_->WaitUntilBlocked());
  icon->ForceVisible();
  auto removed_icon = tray->RemoveStatusIcon(icon);
  tray.reset();
  shell_->Release();
  ASSERT_TRUE(shell_->WaitForPendingTasks());
  EXPECT_TRUE(IsWindow(window));
  // The reply ran with a live icon but a destroyed tray. Deleting the remaining
  // icon must still issue its final Shell deletion before releasing the HWND.
  removed_icon.reset();
  ASSERT_TRUE(base::test::RunUntil([&] { return !IsWindow(window); }));
  EXPECT_FALSE(IsWindow(window));
  const auto calls = shell_->calls();
  ASSERT_FALSE(calls.empty());
  EXPECT_EQ(NIM_DELETE, calls.back().message);
}
