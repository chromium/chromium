// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/util_win/uia_window_identity.h"

#include <windows.h>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/process/process.h"
#include "base/test/bind.h"
#include "base/test/multiprocess_test.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/win/message_window.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace {

constexpr DWORD kProcessAccess = PROCESS_QUERY_LIMITED_INFORMATION;

base::Process OpenWithExpectedAccess(base::ProcessId pid) {
  return base::Process::OpenWithAccess(pid, kProcessAccess);
}

base::Process OpenCurrentProcess() {
  return OpenWithExpectedAccess(::GetCurrentProcessId());
}

// A window of a class that is not Chrome's.
class PlainWindow {
 public:
  static constexpr wchar_t kClass[] = L"UiaWindowIdentityTestWindow";

  PlainWindow() {
    WNDCLASSEXW window_class = {sizeof(window_class)};
    window_class.lpfnWndProc = &::DefWindowProcW;
    window_class.hInstance = ::GetModuleHandle(nullptr);
    window_class.lpszClassName = kClass;
    CHECK(::RegisterClassExW(&window_class));

    hwnd_ =
        ::CreateWindowExW(0, kClass, L"", WS_OVERLAPPED, 0, 0, 10, 10, nullptr,
                          nullptr, window_class.hInstance, nullptr);
    CHECK(hwnd_);
  }

  PlainWindow(const PlainWindow&) = delete;
  PlainWindow& operator=(const PlainWindow&) = delete;

  ~PlainWindow() {
    ::DestroyWindow(hwnd_);
    ::UnregisterClassW(kClass, ::GetModuleHandle(nullptr));
  }

  HWND hwnd() const { return hwnd_; }

 private:
  HWND hwnd_;
};

// Terminated by the test.
MULTIPROCESS_TEST_MAIN(UiaWindowIdentityLiveProcess) {
  base::PlatformThread::Sleep(base::Minutes(1));
  return 0;
}

MULTIPROCESS_TEST_MAIN(UiaWindowIdentityExitingProcess) {
  return 0;
}

TEST(UiaWindowIdentityClassTest, ChromeWindowClasses) {
  EXPECT_TRUE(IsChromeWindowClass(L"Chrome_WidgetWin_1"));
  EXPECT_TRUE(IsChromeWindowClass(L"Chrome_RenderWidgetHostHWND"));
  EXPECT_TRUE(IsChromeWindowClass(L"Chrome_MessageWindow"));
  EXPECT_TRUE(IsChromeWindowClass(L"chrome_widgetwin_1"));
  EXPECT_FALSE(IsChromeWindowClass(L"ApplicationFrameWindow"));
  EXPECT_FALSE(IsChromeWindowClass(L""));
}

using UiaWindowIdentityTest = base::MultiProcessTest;

TEST_F(UiaWindowIdentityTest, MatchingWindow) {
  PlainWindow window;
  EXPECT_TRUE(IsExpectedWindow(window.hwnd(), OpenCurrentProcess(),
                               PlainWindow::kClass));
}

TEST_F(UiaWindowIdentityTest, WrongProcess) {
  base::Process child = SpawnChild("UiaWindowIdentityLiveProcess");
  ASSERT_TRUE(child.IsValid());
  base::ScopedClosureRunner terminate_child(base::BindLambdaForTesting(
      [&child] { child.Terminate(0, /*wait=*/true); }));
  base::Process other = OpenWithExpectedAccess(child.Pid());
  ASSERT_TRUE(other.IsValid());

  PlainWindow window;
  EXPECT_FALSE(IsExpectedWindow(window.hwnd(), other, PlainWindow::kClass));
}

// An exited process owns no windows, and its pid is not reused while `child`
// is held.
TEST_F(UiaWindowIdentityTest, ExitedProcess) {
  base::Process child = SpawnChild("UiaWindowIdentityExitingProcess");
  ASSERT_TRUE(child.IsValid());
  int exit_code = -1;
  ASSERT_TRUE(child.WaitForExit(&exit_code));

  PlainWindow window;
  EXPECT_FALSE(IsExpectedWindow(window.hwnd(), child, PlainWindow::kClass));
}

TEST_F(UiaWindowIdentityTest, HandleWithoutQueryAccess) {
  base::Process process =
      base::Process::OpenWithAccess(::GetCurrentProcessId(), SYNCHRONIZE);
  ASSERT_TRUE(process.IsValid());

  PlainWindow window;
  EXPECT_FALSE(IsExpectedWindow(window.hwnd(), process, PlainWindow::kClass));
}

TEST_F(UiaWindowIdentityTest, WrongClass) {
  PlainWindow window;
  EXPECT_FALSE(
      IsExpectedWindow(window.hwnd(), OpenCurrentProcess(), L"SomethingElse"));
}

TEST_F(UiaWindowIdentityTest, UnidentifiedProcess) {
  PlainWindow window;
  EXPECT_FALSE(
      IsExpectedWindow(window.hwnd(), base::Process(), PlainWindow::kClass));
}

TEST_F(UiaWindowIdentityTest, DestroyedWindow) {
  HWND hwnd;
  {
    PlainWindow window;
    hwnd = window.hwnd();
  }

  ASSERT_FALSE(::IsWindow(hwnd));
  EXPECT_FALSE(
      IsExpectedWindow(hwnd, OpenCurrentProcess(), PlainWindow::kClass));
}

TEST_F(UiaWindowIdentityTest, ChromeWindowEvenWhenItMatches) {
  base::win::MessageWindow window;
  ASSERT_TRUE(window.Create(base::BindRepeating(
      [](UINT, WPARAM, LPARAM, LRESULT*) { return false; })));

  EXPECT_FALSE(IsExpectedWindow(window.hwnd(), OpenCurrentProcess(),
                                L"Chrome_MessageWindow"));

  // Nor in another case, which would otherwise match it.
  EXPECT_FALSE(IsExpectedWindow(window.hwnd(), OpenCurrentProcess(),
                                L"chrome_messagewindow"));
}

}  // namespace
