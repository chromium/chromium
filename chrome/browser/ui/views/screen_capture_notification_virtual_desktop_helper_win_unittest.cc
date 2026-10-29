// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/screen_capture_notification_virtual_desktop_helper_win.h"

#include <shobjidl.h>
#include <windows.h>

#include <wrl/client.h>
#include <wrl/implements.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/debug/dump_without_crashing.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions_win.h"
#include "base/test/scoped_os_info_override_win.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/widget/widget.h"
#include "ui/views/win/hwnd_util.h"

namespace {

constexpr wchar_t kCurrentVirtualDesktopRegValueName[] =
    L"CurrentVirtualDesktop";

constexpr GUID kDesktop1 = {0x11111111,
                            0x2222,
                            0x3333,
                            {0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb}};
constexpr GUID kDesktop2 = {0x22222222,
                            0x3333,
                            0x4444,
                            {0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc}};

class FakeVirtualDesktopManager
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IVirtualDesktopManager> {
 public:
  FakeVirtualDesktopManager() = default;
  ~FakeVirtualDesktopManager() override = default;

  IFACEMETHODIMP IsWindowOnCurrentVirtualDesktop(
      HWND top_level_window,
      BOOL* on_current_desktop) override {
    if (!on_current_desktop) {
      return E_POINTER;
    }
    *on_current_desktop = on_current_desktop_;
    return S_OK;
  }

  IFACEMETHODIMP GetWindowDesktopId(HWND top_level_window,
                                    GUID* desktop_id) override {
    if (!desktop_id) {
      return E_POINTER;
    }
    *desktop_id = last_desktop_id_;
    return S_OK;
  }

  IFACEMETHODIMP MoveWindowToDesktop(HWND top_level_window,
                                     REFGUID desktop_id) override {
    last_moved_hwnd_ = top_level_window;
    last_desktop_id_ = desktop_id;
    ++move_call_count_;
    if (on_move_callback_) {
      std::move(on_move_callback_).Run();
    }
    return S_OK;
  }

  HWND last_moved_hwnd() const { return last_moved_hwnd_; }
  const GUID& last_desktop_id() const { return last_desktop_id_; }
  int move_call_count() const { return move_call_count_; }

  void set_on_current_desktop(BOOL on_current_desktop) {
    on_current_desktop_ = on_current_desktop;
  }

  void set_on_move_callback(base::OnceClosure callback) {
    on_move_callback_ = std::move(callback);
  }

 private:
  BOOL on_current_desktop_ = TRUE;
  HWND last_moved_hwnd_ = nullptr;
  GUID last_desktop_id_ = kDesktop1;
  int move_call_count_ = 0;
  base::OnceClosure on_move_callback_;
};

class ScreenCaptureNotificationVirtualDesktopHelperWinTest
    : public ChromeViewsTestBase,
      public testing::WithParamInterface<
          base::test::ScopedOSInfoOverride::Type> {
 public:
  void SetUp() override {
    set_native_widget_type(NativeWidgetType::kDesktop);
    ChromeViewsTestBase::SetUp();
    os_override_ =
        std::make_unique<base::test::ScopedOSInfoOverride>(GetParam());
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_CURRENT_USER));
    dump_count_ = 0;
    base::debug::ResetDumpWithoutCrashingThrottlingForTesting();
    base::debug::SetDumpWithoutCrashingFunction(&RecordDump);
    fake_vdm_ = Microsoft::WRL::Make<FakeVirtualDesktopManager>();
  }

  void TearDown() override {
    helper_.reset();
    widget_.reset();
    base::debug::SetDumpWithoutCrashingFunction(nullptr);
    base::debug::ResetDumpWithoutCrashingThrottlingForTesting();
    ChromeViewsTestBase::TearDown();
  }

  // Creates and shows a top-level widget managed by the helper.
  void CreateWidgetAndHelper() {
    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET,
                               views::Widget::InitParams::TYPE_WINDOW);
    helper_ =
        std::make_unique<ScreenCaptureNotificationVirtualDesktopHelperWin>(
            widget_.get());
    helper_->SetVirtualDesktopManagerForTesting(fake_vdm_);
    widget_->Show();
    hwnd_ = views::HWNDForWidget(widget_.get());
    ASSERT_TRUE(::IsWindow(hwnd_));
  }

  // Creates the parent of the `VirtualDesktops` key, without the key itself.
  void CreateVirtualDesktopsParentKey() {
    const std::optional<std::wstring> parent_path =
        ScreenCaptureNotificationVirtualDesktopHelperWin::
            GetVirtualDesktopsParentRegKeyPath();
    ASSERT_TRUE(parent_path.has_value());
    base::win::RegKey key;
    ASSERT_EQ(key.Create(HKEY_CURRENT_USER, parent_path->c_str(), KEY_WRITE),
              ERROR_SUCCESS);
  }

  // Creates the `VirtualDesktops` key if needed, and writes `desktop_id` as the
  // current virtual desktop.
  void WriteCurrentVirtualDesktop(const GUID& desktop_id) {
    const std::optional<std::wstring> parent_path =
        ScreenCaptureNotificationVirtualDesktopHelperWin::
            GetVirtualDesktopsParentRegKeyPath();
    ASSERT_TRUE(parent_path.has_value());
    const std::wstring key_path =
        base::StrCat({*parent_path, L"\\VirtualDesktops"});
    base::win::RegKey key;
    ASSERT_EQ(key.Create(HKEY_CURRENT_USER, key_path.c_str(), KEY_WRITE),
              ERROR_SUCCESS);
    ASSERT_EQ(key.WriteValue(kCurrentVirtualDesktopRegValueName, &desktop_id,
                             sizeof(desktop_id), REG_BINARY),
              ERROR_SUCCESS);
  }

  // Writes `desktop_id` as the current virtual desktop and waits for the
  // helper to move the window in response.
  void SwitchVirtualDesktopAndWaitForMove(const GUID& desktop_id) {
    base::RunLoop run_loop;
    fake_vdm_->set_on_move_callback(run_loop.QuitClosure());
    WriteCurrentVirtualDesktop(desktop_id);
    run_loop.Run();
  }

  LONG GetExStyle() const { return ::GetWindowLong(hwnd_, GWL_EXSTYLE); }

  static int dump_count() { return dump_count_; }

 protected:
  Microsoft::WRL::ComPtr<FakeVirtualDesktopManager> fake_vdm_;
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<ScreenCaptureNotificationVirtualDesktopHelperWin> helper_;
  HWND hwnd_ = nullptr;

 private:
  static void RecordDump() { ++dump_count_; }

  static int dump_count_;
  std::unique_ptr<base::test::ScopedOSInfoOverride> os_override_;
  registry_util::RegistryOverrideManager registry_override_manager_;
};

int ScreenCaptureNotificationVirtualDesktopHelperWinTest::dump_count_ = 0;

// Verifies the expected Explorer registry path on Windows 10 (per-session) and
// Windows 11+ (persistent), since the fixture shares
// `GetVirtualDesktopsParentRegKeyPath()` with production code.
TEST_P(ScreenCaptureNotificationVirtualDesktopHelperWinTest,
       ParentRegKeyPathMatchesOSVersion) {
  if (GetParam() == base::test::ScopedOSInfoOverride::Type::kWin11Pro) {
    EXPECT_EQ(ScreenCaptureNotificationVirtualDesktopHelperWin::
                  GetVirtualDesktopsParentRegKeyPath(),
              L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer");
  } else {
    ASSERT_EQ(GetParam(),
              base::test::ScopedOSInfoOverride::Type::kWin10Pro21H1);
    DWORD session_id = 0;
    ASSERT_TRUE(::ProcessIdToSessionId(::GetCurrentProcessId(), &session_id));
    EXPECT_EQ(ScreenCaptureNotificationVirtualDesktopHelperWin::
                  GetVirtualDesktopsParentRegKeyPath(),
              base::StrCat({L"Software\\Microsoft\\Windows\\CurrentVersion\\"
                            L"Explorer\\SessionInfo\\",
                            base::NumberToWString(session_id)}));
  }
}

// The bar is a tool window (shown on all virtual desktops) while visible, and
// a regular taskbar window while minimized.
TEST_P(ScreenCaptureNotificationVirtualDesktopHelperWinTest,
       ToolWindowStyleTogglesOnMinimize) {
  ASSERT_NO_FATAL_FAILURE(CreateWidgetAndHelper());
  EXPECT_NE(GetExStyle() & WS_EX_TOOLWINDOW, 0);
  EXPECT_EQ(GetExStyle() & WS_EX_APPWINDOW, 0);

  widget_->Minimize();
  EXPECT_TRUE(widget_->IsMinimized());
  EXPECT_EQ(GetExStyle() & WS_EX_TOOLWINDOW, 0);
  EXPECT_NE(GetExStyle() & WS_EX_APPWINDOW, 0);

  widget_->Restore();
  EXPECT_FALSE(widget_->IsMinimized());
  EXPECT_NE(GetExStyle() & WS_EX_TOOLWINDOW, 0);
  EXPECT_EQ(GetExStyle() & WS_EX_APPWINDOW, 0);

  // Without registry data, nothing is moved, and since the window is on the
  // current virtual desktop, nothing is reported.
  EXPECT_EQ(fake_vdm_->move_call_count(), 0);
  EXPECT_EQ(dump_count(), 0);
}

// Minimizing moves the window to the current virtual desktop.
TEST_P(ScreenCaptureNotificationVirtualDesktopHelperWinTest,
       MovesToCurrentDesktopOnMinimize) {
  ASSERT_NO_FATAL_FAILURE(WriteCurrentVirtualDesktop(kDesktop2));
  ASSERT_NO_FATAL_FAILURE(CreateWidgetAndHelper());
  EXPECT_EQ(fake_vdm_->move_call_count(), 0);

  widget_->Minimize();
  EXPECT_EQ(fake_vdm_->move_call_count(), 1);
  EXPECT_EQ(fake_vdm_->last_moved_hwnd(), hwnd_);
  EXPECT_EQ(fake_vdm_->last_desktop_id(), kDesktop2);
  EXPECT_EQ(dump_count(), 0);
}

// While minimized, the window follows each virtual desktop switch, which
// verifies that the one-shot registry watcher is re-armed after each change.
TEST_P(ScreenCaptureNotificationVirtualDesktopHelperWinTest,
       FollowsDesktopSwitchesWhileMinimized) {
  ASSERT_NO_FATAL_FAILURE(WriteCurrentVirtualDesktop(kDesktop1));
  ASSERT_NO_FATAL_FAILURE(CreateWidgetAndHelper());
  widget_->Minimize();
  EXPECT_EQ(fake_vdm_->move_call_count(), 1);

  SwitchVirtualDesktopAndWaitForMove(kDesktop2);
  EXPECT_EQ(fake_vdm_->move_call_count(), 2);
  EXPECT_EQ(fake_vdm_->last_moved_hwnd(), hwnd_);
  EXPECT_EQ(fake_vdm_->last_desktop_id(), kDesktop2);

  SwitchVirtualDesktopAndWaitForMove(kDesktop1);
  EXPECT_EQ(fake_vdm_->move_call_count(), 3);
  EXPECT_EQ(fake_vdm_->last_moved_hwnd(), hwnd_);
  EXPECT_EQ(fake_vdm_->last_desktop_id(), kDesktop1);
}

// If the `VirtualDesktops` key does not exist until the first desktop switch,
// the window still follows that switch (and later ones) while minimized.
TEST_P(ScreenCaptureNotificationVirtualDesktopHelperWinTest,
       FollowsFirstDesktopSwitchWhenKeyIsCreatedWhileMinimized) {
  ASSERT_NO_FATAL_FAILURE(CreateVirtualDesktopsParentKey());
  ASSERT_NO_FATAL_FAILURE(CreateWidgetAndHelper());
  widget_->Minimize();
  EXPECT_EQ(fake_vdm_->move_call_count(), 0);

  SwitchVirtualDesktopAndWaitForMove(kDesktop2);
  EXPECT_EQ(fake_vdm_->move_call_count(), 1);
  EXPECT_EQ(fake_vdm_->last_moved_hwnd(), hwnd_);
  EXPECT_EQ(fake_vdm_->last_desktop_id(), kDesktop2);

  SwitchVirtualDesktopAndWaitForMove(kDesktop1);
  EXPECT_EQ(fake_vdm_->move_call_count(), 2);
  EXPECT_EQ(fake_vdm_->last_desktop_id(), kDesktop1);
}

// If the window is not on the current virtual desktop when minimized, but the
// registry value is missing, `DumpWithoutCrashing()` reports that the
// undocumented registry key may have changed.
TEST_P(ScreenCaptureNotificationVirtualDesktopHelperWinTest,
       TriggersDumpWhenRegistryMissingAndOffDesktop) {
  ASSERT_NO_FATAL_FAILURE(CreateWidgetAndHelper());
  fake_vdm_->set_on_current_desktop(FALSE);

  widget_->Minimize();
  EXPECT_TRUE(widget_->IsMinimized());
  EXPECT_EQ(fake_vdm_->move_call_count(), 0);
  EXPECT_EQ(dump_count(), 1);
}

// Closing the widget stops watching the registry.
TEST_P(ScreenCaptureNotificationVirtualDesktopHelperWinTest,
       StopsWatchingWhenWidgetIsClosed) {
  ASSERT_NO_FATAL_FAILURE(WriteCurrentVirtualDesktop(kDesktop1));
  ASSERT_NO_FATAL_FAILURE(CreateWidgetAndHelper());
  EXPECT_TRUE(helper_->IsWatchingRegistryForTesting());

  widget_->CloseNow();
  EXPECT_FALSE(helper_->IsWatchingRegistryForTesting());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ScreenCaptureNotificationVirtualDesktopHelperWinTest,
    testing::Values(base::test::ScopedOSInfoOverride::Type::kWin10Pro21H1,
                    base::test::ScopedOSInfoOverride::Type::kWin11Pro));

}  // namespace
