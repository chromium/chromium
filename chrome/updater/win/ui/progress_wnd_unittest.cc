// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/ui/progress_wnd.h"

#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_reg_util_win.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "base/win/registry.h"
#include "base/win/scoped_gdi_object.h"
#include "base/win/scoped_hdc.h"
#include "chrome/updater/test/test_scope.h"
#include "chrome/updater/test/unit_test_util.h"
#include "chrome/updater/test/unit_test_util_win.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/test/test_executables.h"
#include "chrome/updater/win/test/test_strings.h"
#include "chrome/updater/win/ui/l10n_util.h"
#include "chrome/updater/win/ui/message_loop.h"
#include "chrome/updater/win/ui/resources/updater_installer_strings.h"
#include "chrome/updater/win/ui/ui_test_util.h"
#include "chrome/updater/win/ui/ui_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace updater::ui {
namespace {

// Maximum length for strings read from UI controls.
constexpr size_t kMaxStringLen = 256;

class MockProgressWndEvents : public ui::ProgressWndEvents {
 public:
  // Overrides for OmahaWndEvents.
  MOCK_METHOD(void, DoClose, (), (override));
  MOCK_METHOD(void, DoExit, (), (override));

  // Overrides for CompleteWndEvents.
  MOCK_METHOD(bool, DoLaunchBrowser, (const std::string& url), (override));

  // Overrides for ui::ProgressWndEvents.
  MOCK_METHOD(bool,
              DoRestartBrowser,
              (bool restart_all_browsers, const std::vector<GURL>& urls),
              (override));
  MOCK_METHOD(bool, DoReboot, (), (override));
  MOCK_METHOD(void, DoCancel, (), (override));
};

using ::updater::test::CreateTestDIB24;

// Writes the setting `IsDarkModeOn()` reads. Requires an active
// `RegistryOverrideManager` for HKEY_CURRENT_USER.
void SetDarkMode(bool dark) {
  base::win::RegKey key;
  ASSERT_EQ(key.Create(HKEY_CURRENT_USER,
                       L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes"
                       L"\\Personalize",
                       KEY_SET_VALUE),
            ERROR_SUCCESS);
  ASSERT_EQ(
      key.WriteValue(L"AppsUseLightTheme", static_cast<DWORD>(dark ? 0 : 1)),
      ERROR_SUCCESS);
  ASSERT_EQ(
      key.WriteValue(L"SystemUsesLightTheme", static_cast<DWORD>(dark ? 0 : 1)),
      ERROR_SUCCESS);
}

}  // namespace

class ProgressWndTest : public ui::ProgressWndEvents, public ::testing::Test {
 public:
  // Overrides for OmahaWndEvents.
  void DoClose() override { mock_progress_wnd_events_->DoClose(); }
  void DoExit() override { mock_progress_wnd_events_->DoExit(); }

  // Overrides for CompleteWndEvents.
  bool DoLaunchBrowser(const std::string& url) override {
    return mock_progress_wnd_events_->DoLaunchBrowser(url);
  }

  // Overrides for ProgressWndEvents.
  bool DoRestartBrowser(bool restart_all_browsers,
                        const std::vector<GURL>& urls) override {
    return mock_progress_wnd_events_->DoRestartBrowser(restart_all_browsers,
                                                       urls);
  }
  bool DoReboot() override { return mock_progress_wnd_events_->DoReboot(); }
  void DoCancel() override { mock_progress_wnd_events_->DoCancel(); }

  std::unique_ptr<ProgressWnd> MakeProgressWindow(MessageLoop* message_loop) {
    auto progress_wnd =
        std::make_unique<ui::ProgressWnd>(message_loop, nullptr);
    progress_wnd->SetEventSink(this);
    progress_wnd->Initialize();
    progress_wnd->Show();
    return progress_wnd;
  }

 protected:
  std::unique_ptr<MockProgressWndEvents> mock_progress_wnd_events_ =
      std::make_unique<MockProgressWndEvents>();
};

TEST_F(ProgressWndTest, ClickedButton) {
  // Calls ProgressWnd::OnComplete then simulates a button push on the dialog.
  auto button_tester = [&](CompletionCodes code, int button_to_push) {
    AppCompletionInfo app_completion_info;
    app_completion_info.post_install_url = GURL("http://some-test-url");
    app_completion_info.completion_code = code;
    ObserverCompletionInfo observer_completion_info;
    observer_completion_info.completion_text = u"some text";
    observer_completion_info.apps_info.push_back(app_completion_info);
    MessageLoop ui_message_loop;
    std::unique_ptr<ProgressWnd> progress_wnd =
        MakeProgressWindow(&ui_message_loop);
    progress_wnd->OnComplete(observer_completion_info);
    const HWND button = ::GetDlgItem(progress_wnd->hwnd(), button_to_push);
    ::SendMessageW(progress_wnd->hwnd(), WM_COMMAND,
                   MAKEWPARAM(button_to_push, BN_CLICKED),
                   reinterpret_cast<LPARAM>(button));
  };
  {
    mock_progress_wnd_events_ = std::make_unique<MockProgressWndEvents>();
    ::testing::InSequence seq;
    EXPECT_CALL(*mock_progress_wnd_events_,
                DoRestartBrowser(
                    false, std::vector<GURL>{GURL("http://some-test-url")}))

        .WillOnce(::testing::Return(true));
    EXPECT_CALL(*mock_progress_wnd_events_, DoExit());
    EXPECT_CALL(*mock_progress_wnd_events_, DoClose());
    button_tester(CompletionCodes::COMPLETION_CODE_RESTART_BROWSER,
                  IDC_BUTTON1);
  }
  {
    mock_progress_wnd_events_ = std::make_unique<MockProgressWndEvents>();
    ::testing::InSequence seq;
    EXPECT_CALL(
        *mock_progress_wnd_events_,
        DoRestartBrowser(true, std::vector<GURL>{GURL("http://some-test-url")}))

        .WillOnce(::testing::Return(true));
    EXPECT_CALL(*mock_progress_wnd_events_, DoExit());
    EXPECT_CALL(*mock_progress_wnd_events_, DoClose());
    button_tester(CompletionCodes::COMPLETION_CODE_RESTART_ALL_BROWSERS,
                  IDC_BUTTON1);
  }
  {
    mock_progress_wnd_events_ = std::make_unique<MockProgressWndEvents>();
    ::testing::InSequence seq;
    EXPECT_CALL(*mock_progress_wnd_events_, DoReboot())

        .WillOnce(::testing::Return(true));
    EXPECT_CALL(*mock_progress_wnd_events_, DoExit());
    EXPECT_CALL(*mock_progress_wnd_events_, DoClose());
    button_tester(CompletionCodes::COMPLETION_CODE_REBOOT, IDC_BUTTON1);
  }

  for (auto completion_code :
       {CompletionCodes::COMPLETION_CODE_RESTART_BROWSER,
        CompletionCodes::COMPLETION_CODE_RESTART_ALL_BROWSERS,
        CompletionCodes::COMPLETION_CODE_REBOOT}) {
    mock_progress_wnd_events_ = std::make_unique<MockProgressWndEvents>();
    ::testing::InSequence seq;
    EXPECT_CALL(*mock_progress_wnd_events_,
                DoRestartBrowser(::testing::_, ::testing::_))
        .Times(0);
    EXPECT_CALL(*mock_progress_wnd_events_, DoReboot()).Times(0);
    EXPECT_CALL(*mock_progress_wnd_events_, DoExit());
    EXPECT_CALL(*mock_progress_wnd_events_, DoClose());
    button_tester(completion_code, IDC_BUTTON2);
  }

  for (auto completion_code : {CompletionCodes::COMPLETION_CODE_SUCCESS,
                               CompletionCodes::COMPLETION_CODE_ERROR}) {
    mock_progress_wnd_events_ = std::make_unique<MockProgressWndEvents>();
    ::testing::InSequence seq;
    EXPECT_CALL(*mock_progress_wnd_events_,
                DoRestartBrowser(::testing::_, ::testing::_))
        .Times(0);
    EXPECT_CALL(*mock_progress_wnd_events_, DoExit());
    EXPECT_CALL(*mock_progress_wnd_events_, DoClose());
    button_tester(completion_code, IDC_CLOSE);
  }
}

TEST_F(ProgressWndTest, OnInstallStopped) {
  mock_progress_wnd_events_ = std::make_unique<MockProgressWndEvents>();
  MessageLoop ui_message_loop;
  std::unique_ptr<ProgressWnd> progress_wnd =
      MakeProgressWindow(&ui_message_loop);
  progress_wnd->OnCheckingForUpdate();
  EXPECT_EQ(progress_wnd->cur_state_,
            ProgressWnd::States::STATE_CHECKING_FOR_UPDATE);
  EXPECT_CALL(*mock_progress_wnd_events_, DoCancel());
  progress_wnd->OnClose(WM_CLOSE, 0, 0);
  EXPECT_TRUE(progress_wnd->is_canceled_);
  progress_wnd->DestroyWindow();
}

TEST_F(ProgressWndTest, MaybeCloseWindow) {
  mock_progress_wnd_events_ = std::make_unique<MockProgressWndEvents>();
  EXPECT_CALL(*mock_progress_wnd_events_, DoCancel()).WillOnce([] {
    ::PostThreadMessage(::GetCurrentThreadId(), WM_QUIT, 0, 0);
  });
  MessageLoop message_loop;
  std::unique_ptr<ProgressWnd> progress_wnd = MakeProgressWindow(&message_loop);
  progress_wnd->MaybeCloseWindow();
  message_loop.Run();
  progress_wnd->DestroyWindow();
}

TEST_F(ProgressWndTest, GetBundleCompletionCode) {
  {
    for (CompletionCodes completion_code :
         {CompletionCodes::COMPLETION_CODE_ERROR,
          CompletionCodes::COMPLETION_CODE_INSTALL_FINISHED_BEFORE_CANCEL}) {
      ObserverCompletionInfo info;
      info.completion_code = completion_code;
      EXPECT_EQ(ProgressWnd::GetBundleCompletionCode(info), completion_code);
    }
  }
  {
    ObserverCompletionInfo info;
    EXPECT_EQ(ProgressWnd::GetBundleCompletionCode(info),
              CompletionCodes::COMPLETION_CODE_EXIT_SILENTLY);
  }
  {
    for (CompletionCodes completion_code :
         {CompletionCodes::COMPLETION_CODE_SUCCESS,
          CompletionCodes::COMPLETION_CODE_EXIT_SILENTLY,
          CompletionCodes::COMPLETION_CODE_RESTART_ALL_BROWSERS,
          CompletionCodes::COMPLETION_CODE_REBOOT,
          CompletionCodes::COMPLETION_CODE_RESTART_BROWSER,
          CompletionCodes::COMPLETION_CODE_RESTART_ALL_BROWSERS_NOTICE_ONLY,
          CompletionCodes::COMPLETION_CODE_REBOOT_NOTICE_ONLY,
          CompletionCodes::COMPLETION_CODE_RESTART_BROWSER_NOTICE_ONLY,
          CompletionCodes::COMPLETION_CODE_LAUNCH_COMMAND,
          CompletionCodes::COMPLETION_CODE_INSTALL_FINISHED_BEFORE_CANCEL}) {
      ObserverCompletionInfo info;
      AppCompletionInfo app_info;
      app_info.completion_code = completion_code;
      info.apps_info.push_back(app_info);
      EXPECT_EQ(ProgressWnd::GetBundleCompletionCode(info), completion_code);
    }
  }
  {
    ObserverCompletionInfo info;

    for (CompletionCodes code : {CompletionCodes::COMPLETION_CODE_SUCCESS,
                                 CompletionCodes::COMPLETION_CODE_EXIT_SILENTLY,
                                 CompletionCodes::COMPLETION_CODE_REBOOT}) {
      AppCompletionInfo app_info;
      app_info.completion_code = code;
      info.apps_info.push_back(app_info);
    }
    EXPECT_EQ(ProgressWnd::GetBundleCompletionCode(info),
              CompletionCodes::COMPLETION_CODE_REBOOT);
  }
}

TEST_F(ProgressWndTest, DeterminePostInstallUrls) {
  for (CompletionCodes code :
       {CompletionCodes::COMPLETION_CODE_RESTART_ALL_BROWSERS,
        CompletionCodes::COMPLETION_CODE_RESTART_BROWSER}) {
    MessageLoop message_loop;
    std::unique_ptr<ProgressWnd> progress_wnd =
        MakeProgressWindow(&message_loop);
    ObserverCompletionInfo observer_completion_info;
    AppCompletionInfo app_completion_info;
    app_completion_info.completion_code = code;
    app_completion_info.post_install_url = GURL("http://some-test-url");
    observer_completion_info.apps_info.push_back(app_completion_info);
    progress_wnd->DeterminePostInstallUrls(observer_completion_info);
    EXPECT_EQ(progress_wnd->post_install_urls_,
              std::vector<GURL>{GURL("http://some-test-url")});
    progress_wnd->DestroyWindow();
  }
}

TEST_F(ProgressWndTest, OnCheckingForUpdate) {
  MessageLoop ui_message_loop;
  std::unique_ptr<ProgressWnd> progress_wnd =
      MakeProgressWindow(&ui_message_loop);
  progress_wnd->OnCheckingForUpdate();
  EXPECT_EQ(progress_wnd->cur_state_,
            ProgressWnd::States::STATE_CHECKING_FOR_UPDATE);
  EXPECT_FALSE(
      ::IsWindowEnabled(::GetDlgItem(progress_wnd->hwnd(), IDC_CLOSE)));
  progress_wnd->DestroyWindow();
}

TEST_F(ProgressWndTest, OnWaitingToDownload) {
  for (const int is_retry : {false, true}) {
    MessageLoop ui_message_loop;
    std::unique_ptr<ProgressWnd> progress_wnd =
        MakeProgressWindow(&ui_message_loop);
    if (is_retry) {
      progress_wnd->OnWaitingRetryDownload(
          "app-id", u"app-name",
          base::Time::NowFromSystemTime() + base::Minutes(5));
    } else {
      progress_wnd->OnWaitingToDownload("app-id", u"app-name");
    }
    EXPECT_EQ(progress_wnd->cur_state_,
              ProgressWnd::States::STATE_WAITING_TO_DOWNLOAD);
    EXPECT_FALSE(
        ::IsWindowEnabled(::GetDlgItem(progress_wnd->hwnd(), IDC_CLOSE)));
    wchar_t state_text[kMaxStringLen] = {};
    ::GetDlgItemTextW(progress_wnd->hwnd(), IDC_INSTALLER_STATE_TEXT,
                      state_text, std::size(state_text));
    EXPECT_STREQ(state_text, L"");
    progress_wnd->DestroyWindow();
  }
}

TEST_F(ProgressWndTest, OnDownloading) {
  struct TestCase {
    const std::optional<base::TimeDelta> time_remaining;
    const bool is_canceled;
    const unsigned int expected_string_id;
  } cases[] = {
      {base::Seconds(20), false, IDS_DOWNLOADING_BASE},
      {base::Minutes(5), false, IDS_DOWNLOADING_BASE},
      {base::Hours(2), false, IDS_DOWNLOADING_BASE},
      {std::nullopt, false, IDS_DOWNLOADING_BASE},
      {base::Seconds(0), false, IDS_DOWNLOADING_COMPLETED_BASE},
      {base::Seconds(20), true, IDS_CANCELING_BASE},
  };

  MessageLoop ui_message_loop;
  std::unique_ptr<ProgressWnd> progress_wnd =
      MakeProgressWindow(&ui_message_loop);

  for (const auto& test_case : cases) {
    progress_wnd->is_canceled_ = test_case.is_canceled;
    progress_wnd->OnDownloading("app-id", u"app-name", test_case.time_remaining,
                                50);
    EXPECT_EQ(progress_wnd->cur_state_, ProgressWnd::States::STATE_DOWNLOADING);
    EXPECT_FALSE(
        ::IsWindowEnabled(::GetDlgItem(progress_wnd->hwnd(), IDC_CLOSE)));
    wchar_t state_text[kMaxStringLen] = {};
    ::GetDlgItemTextW(progress_wnd->hwnd(), IDC_INSTALLER_STATE_TEXT,
                      state_text, std::size(state_text));
    EXPECT_STREQ(state_text,
                 GetLocalizedString(test_case.expected_string_id).c_str());
  }

  progress_wnd->DestroyWindow();
}

TEST_F(ProgressWndTest, OnPause) {
  MessageLoop ui_message_loop;
  std::unique_ptr<ProgressWnd> progress_wnd =
      MakeProgressWindow(&ui_message_loop);
  progress_wnd->OnPause();
  EXPECT_EQ(progress_wnd->cur_state_, ProgressWnd::States::STATE_PAUSED);
  progress_wnd->DestroyWindow();
}

TEST_F(ProgressWndTest, OnComplete) {
  using ::testing::AnyNumber;
  EXPECT_CALL(*mock_progress_wnd_events_, DoExit()).Times(AnyNumber());
  EXPECT_CALL(*mock_progress_wnd_events_, DoClose()).Times(AnyNumber());

  MessageLoop ui_message_loop;
  {
    std::unique_ptr<ProgressWnd> progress_wnd =
        MakeProgressWindow(&ui_message_loop);
    ObserverCompletionInfo observer_completion_info;
    progress_wnd->OnComplete(observer_completion_info);
    EXPECT_EQ(progress_wnd->cur_state_,
              ProgressWnd::States::STATE_COMPLETE_SUCCESS);
  }
  {
    std::unique_ptr<ProgressWnd> progress_wnd =
        MakeProgressWindow(&ui_message_loop);
    AppCompletionInfo app_completion_info;
    app_completion_info.completion_code =
        CompletionCodes::COMPLETION_CODE_SUCCESS;
    ObserverCompletionInfo observer_completion_info;
    observer_completion_info.completion_text = u"text";
    observer_completion_info.apps_info.push_back(app_completion_info);
    progress_wnd->OnComplete(observer_completion_info);
    wchar_t completion_text[kMaxStringLen] = {};
    ::GetDlgItemTextW(progress_wnd->hwnd(), IDC_COMPLETE_TEXT, completion_text,
                      std::size(completion_text));
    EXPECT_STREQ(completion_text, L"text");
    EXPECT_TRUE(
        ::IsWindowEnabled(::GetDlgItem(progress_wnd->hwnd(), IDC_CLOSE)));
    progress_wnd->DestroyWindow();
  }
}

TEST_F(ProgressWndTest, LaunchCmdLine) {
  using ::testing::AnyNumber;
  EXPECT_CALL(*mock_progress_wnd_events_, DoExit()).Times(AnyNumber());
  EXPECT_CALL(*mock_progress_wnd_events_, DoClose()).Times(AnyNumber());

  // Create a shared event to be waited for in this process and signaled in the
  // test process. If the test is running elevated with UAC on, the test will
  // also confirm that the test process is launched at medium integrity, by
  // creating an event with a security descriptor that allows the medium
  // integrity process to signal it.
  test::EventHolder event_holder(
      IsElevatedWithUACOn() ? test::CreateEveryoneWaitableEventForTest()
                            : test::CreateWaitableEventForTest());
  ASSERT_NE(event_holder.event.handle(), nullptr);

  base::CommandLine test_process_cmd_line = GetTestProcessCommandLine(
      GetUpdaterScopeForTesting(), test::GetTestName());
  test_process_cmd_line.AppendSwitchNative(
      IsElevatedWithUACOn() ? kTestEventToSignalIfMediumIntegrity
                            : kTestEventToSignal,
      event_holder.name);
  MessageLoop ui_message_loop;
  std::unique_ptr<ProgressWnd> progress_wnd =
      MakeProgressWindow(&ui_message_loop);
  AppCompletionInfo app_completion_info;
  app_completion_info.completion_code =
      CompletionCodes::COMPLETION_CODE_EXIT_SILENTLY_ON_LAUNCH_COMMAND;
  app_completion_info.post_install_launch_command_line =
      base::WideToUTF8(test_process_cmd_line.GetCommandLineString());
  ObserverCompletionInfo observer_completion_info;
  observer_completion_info.completion_text = u"text";
  observer_completion_info.apps_info.push_back(app_completion_info);
  progress_wnd->OnComplete(observer_completion_info);

  EXPECT_TRUE(event_holder.event.TimedWait(TestTimeouts::action_max_timeout()));
  EXPECT_TRUE(test::WaitFor(
      [] { return test::FindProcesses(kTestProcessExecutableName).empty(); }));
}

TEST_F(ProgressWndTest, FlatButtonSubclass) {
  MessageLoop ui_message_loop;
  std::unique_ptr<ProgressWnd> progress_wnd =
      MakeProgressWindow(&ui_message_loop);

  EXPECT_EQ(progress_wnd->btn1_.hwnd(),
            ::GetDlgItem(progress_wnd->hwnd(), IDC_BUTTON1));
  EXPECT_TRUE(progress_wnd->btn1_.IsWindow());

  EXPECT_EQ(progress_wnd->btn2_.hwnd(),
            ::GetDlgItem(progress_wnd->hwnd(), IDC_BUTTON2));
  EXPECT_TRUE(progress_wnd->btn2_.IsWindow());

  EXPECT_EQ(progress_wnd->close_btn_.hwnd(),
            ::GetDlgItem(progress_wnd->hwnd(), IDC_CLOSE));
  EXPECT_TRUE(progress_wnd->close_btn_.IsWindow());

  EXPECT_EQ(progress_wnd->get_help_btn_.hwnd(),
            ::GetDlgItem(progress_wnd->hwnd(), IDC_GET_HELP));
  EXPECT_TRUE(progress_wnd->get_help_btn_.IsWindow());

  progress_wnd->DestroyWindow();
}

// Verifies that the app logo control dynamically resizes to match both
// theme-specific square logos (48x48) and legacy rectangular logos (92x24),
// scaling correctly under the window's effective DPI while keeping the bottom
// edge aligned with the layout baseline.
TEST_F(ProgressWndTest, SetAppLogoDynamicSizing) {
  MessageLoop ui_message_loop;
  std::unique_ptr<ProgressWnd> progress_wnd =
      MakeProgressWindow(&ui_message_loop);

  const HWND app_bitmap_ctl =
      ::GetDlgItem(progress_wnd->hwnd(), IDC_APP_BITMAP);
  base::win::ScopedGetDC dc(nullptr);

  // Test with a 48x48 square logo.
  base::win::ScopedGDIObject<HBITMAP> square_bitmap =
      CreateTestDIB24(dc, 48, 48);
  EXPECT_TRUE(square_bitmap.is_valid());

  ::SendMessage(progress_wnd->hwnd(), WM_SET_APP_LOGO,
                reinterpret_cast<WPARAM>(square_bitmap.release()), 0);

  RECT ctl_rect = progress_wnd->GetControlClientRect(app_bitmap_ctl);
  const int initial_bottom = ctl_rect.bottom;
  const int dpi = ::GetDpiForWindow(progress_wnd->hwnd());
  const int effective_dpi = dpi ? dpi : USER_DEFAULT_SCREEN_DPI;
  EXPECT_EQ(ctl_rect.right - ctl_rect.left,
            ::MulDiv(48, effective_dpi, USER_DEFAULT_SCREEN_DPI));
  EXPECT_EQ(ctl_rect.bottom - ctl_rect.top,
            ::MulDiv(48, effective_dpi, USER_DEFAULT_SCREEN_DPI));

  // Test with a 92x24 rectangular logo.
  base::win::ScopedGDIObject<HBITMAP> rect_bitmap = CreateTestDIB24(dc, 92, 24);
  EXPECT_TRUE(rect_bitmap.is_valid());

  ::SendMessage(progress_wnd->hwnd(), WM_SET_APP_LOGO,
                reinterpret_cast<WPARAM>(rect_bitmap.release()), 0);

  // Verify that the rectangular logo matches expected scaled dimensions and
  // the bottom coordinate remains locked to the original baseline.
  ctl_rect = progress_wnd->GetControlClientRect(app_bitmap_ctl);
  EXPECT_EQ(ctl_rect.right - ctl_rect.left,
            ::MulDiv(92, effective_dpi, USER_DEFAULT_SCREEN_DPI));
  EXPECT_EQ(ctl_rect.bottom - ctl_rect.top,
            ::MulDiv(24, effective_dpi, USER_DEFAULT_SCREEN_DPI));
  EXPECT_EQ(ctl_rect.bottom, initial_bottom);

  progress_wnd->DestroyWindow();
}

// Verifies that caching both light and dark logos allows switching the
// displayed logo when a theme change (WM_SETTINGCHANGE or WM_SYSCOLORCHANGE)
// occurs without requiring redownloading or resetting the cache.
TEST_F(ProgressWndTest, SetAppLogoThemeSwitching) {
  if (IsHighContrastOn()) {
    GTEST_SKIP();
  }
  registry_util::RegistryOverrideManager registry_override;
  ASSERT_NO_FATAL_FAILURE(
      registry_override.OverrideRegistry(HKEY_CURRENT_USER));

  // Start with light mode.
  SetDarkMode(false);

  MessageLoop ui_message_loop;
  std::unique_ptr<ProgressWnd> progress_wnd =
      MakeProgressWindow(&ui_message_loop);

  const HWND app_bitmap_ctl =
      ::GetDlgItem(progress_wnd->hwnd(), IDC_APP_BITMAP);
  base::win::ScopedGetDC dc(nullptr);

  // Light logo: 32x32, Dark logo: 48x48.
  base::win::ScopedGDIObject<HBITMAP> light_bitmap =
      CreateTestDIB24(dc, 32, 32);
  base::win::ScopedGDIObject<HBITMAP> dark_bitmap = CreateTestDIB24(dc, 48, 48);
  EXPECT_TRUE(light_bitmap.is_valid());
  EXPECT_TRUE(dark_bitmap.is_valid());

  const HBITMAP light_hbitmap = light_bitmap.get();
  const HBITMAP dark_hbitmap = dark_bitmap.get();

  const HICON initial_big_icon = reinterpret_cast<HICON>(
      ::SendMessage(progress_wnd->hwnd(), WM_GETICON, ICON_BIG, 0));
  const HICON initial_small_icon = reinterpret_cast<HICON>(
      ::SendMessage(progress_wnd->hwnd(), WM_GETICON, ICON_SMALL, 0));
  EXPECT_NE(initial_big_icon, nullptr);
  EXPECT_NE(initial_small_icon, nullptr);

  ::SendMessage(progress_wnd->hwnd(), WM_SET_APP_LOGO,
                reinterpret_cast<WPARAM>(light_bitmap.release()),
                reinterpret_cast<LPARAM>(dark_bitmap.release()));

  const HICON custom_big_icon = reinterpret_cast<HICON>(
      ::SendMessage(progress_wnd->hwnd(), WM_GETICON, ICON_BIG, 0));
  const HICON custom_small_icon = reinterpret_cast<HICON>(
      ::SendMessage(progress_wnd->hwnd(), WM_GETICON, ICON_SMALL, 0));
  EXPECT_NE(custom_big_icon, nullptr);
  EXPECT_NE(custom_small_icon, nullptr);
  EXPECT_NE(custom_big_icon, initial_big_icon);
  EXPECT_NE(custom_small_icon, initial_small_icon);

  EXPECT_EQ(progress_wnd->light_app_logo_bmp_.get(), light_hbitmap);
  EXPECT_EQ(progress_wnd->dark_app_logo_bmp_.get(), dark_hbitmap);
  EXPECT_EQ(progress_wnd->GetCurrentAppLogoBitmap(), light_hbitmap);

  const int dpi = ::GetDpiForWindow(progress_wnd->hwnd());
  const int effective_dpi = dpi ? dpi : USER_DEFAULT_SCREEN_DPI;

  RECT ctl_rect = progress_wnd->GetControlClientRect(app_bitmap_ctl);
  EXPECT_EQ(ctl_rect.right - ctl_rect.left,
            ::MulDiv(32, effective_dpi, USER_DEFAULT_SCREEN_DPI));
  EXPECT_EQ(ctl_rect.bottom - ctl_rect.top,
            ::MulDiv(32, effective_dpi, USER_DEFAULT_SCREEN_DPI));

  // Switch to dark mode and notify the window via WM_SETTINGCHANGE.
  SetDarkMode(true);
  ::SendMessage(progress_wnd->hwnd(), WM_SETTINGCHANGE, 0,
                reinterpret_cast<LPARAM>(L"ImmersiveColorSet"));

  EXPECT_EQ(progress_wnd->GetCurrentAppLogoBitmap(), dark_hbitmap);
  const HICON dark_big_icon = reinterpret_cast<HICON>(
      ::SendMessage(progress_wnd->hwnd(), WM_GETICON, ICON_BIG, 0));
  EXPECT_NE(dark_big_icon, nullptr);
  EXPECT_NE(dark_big_icon, custom_big_icon);
  ctl_rect = progress_wnd->GetControlClientRect(app_bitmap_ctl);
  EXPECT_EQ(ctl_rect.right - ctl_rect.left,
            ::MulDiv(48, effective_dpi, USER_DEFAULT_SCREEN_DPI));
  EXPECT_EQ(ctl_rect.bottom - ctl_rect.top,
            ::MulDiv(48, effective_dpi, USER_DEFAULT_SCREEN_DPI));

  // Switch back to light mode and notify via WM_SYSCOLORCHANGE.
  SetDarkMode(false);
  ::SendMessage(progress_wnd->hwnd(), WM_SYSCOLORCHANGE, 0, 0);

  EXPECT_EQ(progress_wnd->GetCurrentAppLogoBitmap(), light_hbitmap);
  const HICON light_big_icon = reinterpret_cast<HICON>(
      ::SendMessage(progress_wnd->hwnd(), WM_GETICON, ICON_BIG, 0));
  EXPECT_NE(light_big_icon, nullptr);
  EXPECT_NE(light_big_icon, dark_big_icon);
  ctl_rect = progress_wnd->GetControlClientRect(app_bitmap_ctl);
  EXPECT_EQ(ctl_rect.right - ctl_rect.left,
            ::MulDiv(32, effective_dpi, USER_DEFAULT_SCREEN_DPI));
  EXPECT_EQ(ctl_rect.bottom - ctl_rect.top,
            ::MulDiv(32, effective_dpi, USER_DEFAULT_SCREEN_DPI));

  // Test with only a single fallback logo (light provided, dark is null).
  base::win::ScopedGDIObject<HBITMAP> fallback_bitmap =
      CreateTestDIB24(dc, 64, 64);
  EXPECT_TRUE(fallback_bitmap.is_valid());
  const HBITMAP fallback_hbitmap = fallback_bitmap.get();

  ::SendMessage(progress_wnd->hwnd(), WM_SET_APP_LOGO,
                reinterpret_cast<WPARAM>(fallback_bitmap.release()), 0);

  EXPECT_NE(reinterpret_cast<HICON>(
                ::SendMessage(progress_wnd->hwnd(), WM_GETICON, ICON_BIG, 0)),
            nullptr);
  EXPECT_NE(reinterpret_cast<HICON>(
                ::SendMessage(progress_wnd->hwnd(), WM_GETICON, ICON_SMALL, 0)),
            nullptr);

  EXPECT_EQ(progress_wnd->light_app_logo_bmp_.get(), fallback_hbitmap);
  EXPECT_EQ(progress_wnd->dark_app_logo_bmp_.get(), nullptr);

  // In light mode, uses fallback logo.
  EXPECT_EQ(progress_wnd->GetCurrentAppLogoBitmap(), fallback_hbitmap);
  ctl_rect = progress_wnd->GetControlClientRect(app_bitmap_ctl);
  EXPECT_EQ(ctl_rect.right - ctl_rect.left,
            ::MulDiv(64, effective_dpi, USER_DEFAULT_SCREEN_DPI));
  EXPECT_EQ(ctl_rect.bottom - ctl_rect.top,
            ::MulDiv(64, effective_dpi, USER_DEFAULT_SCREEN_DPI));

  // In dark mode with no dark logo provided, falls back to light logo.
  SetDarkMode(true);
  ::SendMessage(progress_wnd->hwnd(), WM_SETTINGCHANGE, 0,
                reinterpret_cast<LPARAM>(L"ImmersiveColorSet"));
  EXPECT_EQ(progress_wnd->GetCurrentAppLogoBitmap(), fallback_hbitmap);
  ctl_rect = progress_wnd->GetControlClientRect(app_bitmap_ctl);
  EXPECT_EQ(ctl_rect.right - ctl_rect.left,
            ::MulDiv(64, effective_dpi, USER_DEFAULT_SCREEN_DPI));
  EXPECT_EQ(ctl_rect.bottom - ctl_rect.top,
            ::MulDiv(64, effective_dpi, USER_DEFAULT_SCREEN_DPI));

  // Switch back to light mode and verify fallback logo persists.
  SetDarkMode(false);
  ::SendMessage(progress_wnd->hwnd(), WM_SYSCOLORCHANGE, 0, 0);
  EXPECT_EQ(progress_wnd->GetCurrentAppLogoBitmap(), fallback_hbitmap);
  ctl_rect = progress_wnd->GetControlClientRect(app_bitmap_ctl);
  EXPECT_EQ(ctl_rect.right - ctl_rect.left,
            ::MulDiv(64, effective_dpi, USER_DEFAULT_SCREEN_DPI));
  EXPECT_EQ(ctl_rect.bottom - ctl_rect.top,
            ::MulDiv(64, effective_dpi, USER_DEFAULT_SCREEN_DPI));

  // Test window icon re-scaling across dynamic DPI changes (WM_DPICHANGED).
  RECT suggested_rect = {0, 0, 600, 400};
  ::SendMessage(progress_wnd->hwnd(), WM_DPICHANGED, MAKELPARAM(192, 192),
                reinterpret_cast<LPARAM>(&suggested_rect));
  EXPECT_NE(reinterpret_cast<HICON>(
                ::SendMessage(progress_wnd->hwnd(), WM_GETICON, ICON_BIG, 0)),
            nullptr);
  EXPECT_NE(reinterpret_cast<HICON>(
                ::SendMessage(progress_wnd->hwnd(), WM_GETICON, ICON_SMALL, 0)),
            nullptr);

  // Test passing duplicate logo handles for both light and dark themes.
  // Verify only one ScopedGDIObject retains ownership to avoid double-free,
  // and fallback logic serves the logo for both themes.
  base::win::ScopedGDIObject<HBITMAP> duplicate_bitmap =
      CreateTestDIB24(dc, 64, 64);
  EXPECT_TRUE(duplicate_bitmap.is_valid());
  const HBITMAP duplicate_hbitmap = duplicate_bitmap.get();
  const HBITMAP released_hbitmap = duplicate_bitmap.release();
  ::SendMessage(progress_wnd->hwnd(), WM_SET_APP_LOGO,
                reinterpret_cast<WPARAM>(duplicate_hbitmap),
                reinterpret_cast<LPARAM>(released_hbitmap));
  EXPECT_EQ(progress_wnd->light_app_logo_bmp_.get(), duplicate_hbitmap);
  EXPECT_EQ(progress_wnd->dark_app_logo_bmp_.get(), nullptr);
  EXPECT_EQ(progress_wnd->GetCurrentAppLogoBitmap(), duplicate_hbitmap);

  // Clear the app logo and verify the window icon falls back to the default
  // app icon (IDI_APP).
  ::SendMessage(progress_wnd->hwnd(), WM_SET_APP_LOGO, 0, 0);
  EXPECT_NE(reinterpret_cast<HICON>(
                ::SendMessage(progress_wnd->hwnd(), WM_GETICON, ICON_BIG, 0)),
            nullptr);
  EXPECT_NE(reinterpret_cast<HICON>(
                ::SendMessage(progress_wnd->hwnd(), WM_GETICON, ICON_SMALL, 0)),
            nullptr);

  progress_wnd->DestroyWindow();
}

// Verifies which WM_SETTINGCHANGE messages reach the theme refresh, and that
// `lparam` is never dereferenced on either the accepted or the rejected path.
// `is_dark_mode()` is the observable: it is the cached state the dialog and
// every descendant control paint from, so it is exactly "UpdateThemeState()
// ran".
TEST_F(ProgressWndTest, SettingChangeFiltering) {
  if (IsHighContrastOn()) {
    // `IsDarkModeOn()` short-circuits to the system window color under high
    // contrast and never reads the overridden key, so the observable below
    // does not respond to `SetDarkMode()`.
    GTEST_SKIP() << "Dark mode is not registry-driven under high contrast";
  }

  registry_util::RegistryOverrideManager registry_override;
  ASSERT_NO_FATAL_FAILURE(
      registry_override.OverrideRegistry(HKEY_CURRENT_USER));
  SetDarkMode(false);

  MessageLoop ui_message_loop;
  std::unique_ptr<ProgressWnd> progress_wnd =
      MakeProgressWindow(&ui_message_loop);
  const HWND hwnd = progress_wnd->hwnd();
  ASSERT_FALSE(progress_wnd->is_dark_mode());

  // Flipped without notifying, so each send below has a transition to make.
  SetDarkMode(true);

  // Rejected: names an action the dialog does not paint from. Passing
  // L"WindowMetrics" mirrors what SystemParametersInfo sends for this action.
  ::SendMessage(hwnd, WM_SETTINGCHANGE, SPI_SETNONCLIENTMETRICS,
                reinterpret_cast<LPARAM>(L"WindowMetrics"));
  EXPECT_FALSE(progress_wnd->is_dark_mode());

  // Accepted: zero wParam is what the shell broadcasts carry for theme changes.
  ::SendMessage(hwnd, WM_SETTINGCHANGE, 0,
                reinterpret_cast<LPARAM>(L"ImmersiveColorSet"));
  EXPECT_TRUE(progress_wnd->is_dark_mode());

  // Accepted: Windows sends the high contrast toggle with `lparam` naming
  // L"HighContrast", which the predicate this CL replaces rejected outright.
  SetDarkMode(false);
  ::SendMessage(hwnd, WM_SETTINGCHANGE, SPI_SETHIGHCONTRAST,
                reinterpret_cast<LPARAM>(L"HighContrast"));
  EXPECT_FALSE(progress_wnd->is_dark_mode());

  progress_wnd->DestroyWindow();
}

// The regression test for the suppression itself: a broadcast that changed
// no theme flag must not reach the refresh at all.
TEST_F(ProgressWndTest, SettingChangeWithoutTransitionDoesNotRefresh) {
  MessageLoop ui_message_loop;
  std::unique_ptr<ProgressWnd> progress_wnd =
      MakeProgressWindow(&ui_message_loop);

  const int before = progress_wnd->theme_refresh_count_for_testing_;
  ::SendMessage(progress_wnd->hwnd(), WM_SETTINGCHANGE, 0,
                reinterpret_cast<LPARAM>(L"ImmersiveColorSet"));
  EXPECT_EQ(progress_wnd->theme_refresh_count_for_testing_, before);

  progress_wnd->DestroyWindow();
}

// WM_THEMECHANGED means "reload", not "the flags moved": it is also the retry
// path after a bitmap load that failed. Pinned so the two paths do not get
// unified back together.
TEST_F(ProgressWndTest, ThemeChangedRefreshesWithoutTransition) {
  MessageLoop ui_message_loop;
  std::unique_ptr<ProgressWnd> progress_wnd =
      MakeProgressWindow(&ui_message_loop);

  const int before = progress_wnd->theme_refresh_count_for_testing_;
  ::SendMessage(progress_wnd->hwnd(), WM_THEMECHANGED, 0, 0);
  EXPECT_EQ(progress_wnd->theme_refresh_count_for_testing_, before + 1);

  progress_wnd->DestroyWindow();
}

// A WM_DPICHANGED without the suggested rect must be tolerated rather than
// dereferenced. The window keeps its bounds; the rescale still runs from
// `wparam`.
TEST_F(ProgressWndTest, DpiChangedWithoutSuggestedRect) {
  MessageLoop ui_message_loop;
  std::unique_ptr<ProgressWnd> progress_wnd =
      MakeProgressWindow(&ui_message_loop);
  const HWND hwnd = progress_wnd->hwnd();

  base::win::ScopedGetDC dc(hwnd);
  base::win::ScopedGDIObject<HBITMAP> logo = CreateTestDIB24(dc, 48, 48);
  ASSERT_TRUE(logo.is_valid());
  ::SendMessage(hwnd, WM_SET_APP_LOGO, reinterpret_cast<WPARAM>(logo.release()),
                0);

  const UINT window_dpi = ::GetDpiForWindow(hwnd);
  const UINT target_dpi = (window_dpi == 192) ? 96 : 192;
  ASSERT_NE(target_dpi, window_dpi);

  RECT before = {};
  ASSERT_TRUE(::GetWindowRect(hwnd, &before));

  ::SendMessage(hwnd, WM_DPICHANGED, MAKEWPARAM(target_dpi, target_dpi), 0);

  RECT after = {};
  ASSERT_TRUE(::GetWindowRect(hwnd, &after));
  EXPECT_TRUE(::EqualRect(&before, &after));

  // The rescale ran despite the missing rect: the window icon is sized for
  // the DPI in `wparam`.
  const HICON big_icon =
      reinterpret_cast<HICON>(::SendMessage(hwnd, WM_GETICON, ICON_BIG, 0));
  ASSERT_NE(big_icon, nullptr);
  ICONINFO big_info = {};
  ASSERT_TRUE(::GetIconInfo(big_icon, &big_info));
  base::win::ScopedGDIObject<HBITMAP> big_color(big_info.hbmColor);
  base::win::ScopedGDIObject<HBITMAP> big_mask(big_info.hbmMask);
  BITMAP bm_big = {};
  ASSERT_NE(::GetObject(big_color.get(), sizeof(bm_big), &bm_big), 0);
  EXPECT_EQ(bm_big.bmWidth, ::GetSystemMetricsForDpi(SM_CXICON, target_dpi));
  EXPECT_NE(bm_big.bmWidth, ::GetSystemMetricsForDpi(SM_CXICON, window_dpi));

  progress_wnd->DestroyWindow();
}

TEST_F(ProgressWndTest, AppLogoScalingFailureFallback) {
  MessageLoop ui_message_loop;
  std::unique_ptr<ProgressWnd> progress_wnd =
      MakeProgressWindow(&ui_message_loop);

  // Dialog initializes with default IDI_APP icons.
  EXPECT_NE(reinterpret_cast<HICON>(
                ::SendMessage(progress_wnd->hwnd(), WM_GETICON, ICON_BIG, 0)),
            nullptr);
  EXPECT_NE(reinterpret_cast<HICON>(
                ::SendMessage(progress_wnd->hwnd(), WM_GETICON, ICON_SMALL, 0)),
            nullptr);

  // Set a valid logo so custom derived window icons are active.
  base::win::ScopedGetDC dc(progress_wnd->hwnd());
  base::win::ScopedGDIObject<HBITMAP> valid_logo = CreateTestDIB24(dc, 48, 48);
  ASSERT_TRUE(valid_logo.is_valid());
  ::SendMessage(progress_wnd->hwnd(), WM_SET_APP_LOGO,
                reinterpret_cast<WPARAM>(valid_logo.release()), 0);
  EXPECT_NE(reinterpret_cast<HICON>(
                ::SendMessage(progress_wnd->hwnd(), WM_GETICON, ICON_BIG, 0)),
            nullptr);
  EXPECT_NE(reinterpret_cast<HICON>(
                ::SendMessage(progress_wnd->hwnd(), WM_GETICON, ICON_SMALL, 0)),
            nullptr);

  // Simulate a logo update where bitmap scaling or icon creation fails
  // (e.g. source bitmap is locked into another device context).
  base::win::ScopedCreateDC lock_dc(::CreateCompatibleDC(dc));
  ASSERT_TRUE(lock_dc.is_valid());
  base::win::ScopedGDIObject<HBITMAP> locked_logo = CreateTestDIB24(dc, 48, 48);
  ASSERT_TRUE(locked_logo.is_valid());
  const HBITMAP locked_hbitmap = locked_logo.get();
  HGDIOBJ old_selected = ::SelectObject(lock_dc.get(), locked_hbitmap);
  ASSERT_TRUE(old_selected && old_selected != HGDI_ERROR);

  ::SendMessage(progress_wnd->hwnd(), WM_SET_APP_LOGO,
                reinterpret_cast<WPARAM>(locked_logo.release()), 0);

  // Verify WM_GETICON returns valid IDI_APP fallback handles rather than null
  // or destroyed handles.
  const HICON fallback_big_icon = reinterpret_cast<HICON>(
      ::SendMessage(progress_wnd->hwnd(), WM_GETICON, ICON_BIG, 0));
  const HICON fallback_small_icon = reinterpret_cast<HICON>(
      ::SendMessage(progress_wnd->hwnd(), WM_GETICON, ICON_SMALL, 0));
  EXPECT_NE(fallback_big_icon, nullptr);
  EXPECT_NE(fallback_small_icon, nullptr);

  // Deselect from lock_dc before progress_wnd destroys the bitmap to ensure
  // clean GDI object deletion.
  ::SelectObject(lock_dc.get(), old_selected);

  // Now that the bitmap is unlocked, dispatching WM_THEMECHANGED invokes
  // UpdateAppLogo(). Because current_logo_ was cleared on fallback, the method
  // retries icon creation rather than early-returning due to a stale cache hit.
  ::SendMessage(progress_wnd->hwnd(), WM_THEMECHANGED, 0, 0);
  const HICON retried_big_icon = reinterpret_cast<HICON>(
      ::SendMessage(progress_wnd->hwnd(), WM_GETICON, ICON_BIG, 0));
  const HICON retried_small_icon = reinterpret_cast<HICON>(
      ::SendMessage(progress_wnd->hwnd(), WM_GETICON, ICON_SMALL, 0));
  EXPECT_NE(retried_big_icon, nullptr);
  EXPECT_NE(retried_small_icon, nullptr);
  EXPECT_NE(retried_big_icon, fallback_big_icon);
  EXPECT_NE(retried_small_icon, fallback_small_icon);

  progress_wnd->DestroyWindow();
}

TEST_F(ProgressWndTest, ApplyDpiScalingIconMetrics) {
  MessageLoop ui_message_loop;
  std::unique_ptr<ProgressWnd> progress_wnd =
      MakeProgressWindow(&ui_message_loop);

  base::win::ScopedGetDC dc(progress_wnd->hwnd());
  base::win::ScopedGDIObject<HBITMAP> logo = CreateTestDIB24(dc, 48, 48);
  ASSERT_TRUE(logo.is_valid());
  ::SendMessage(progress_wnd->hwnd(), WM_SET_APP_LOGO,
                reinterpret_cast<WPARAM>(logo.release()), 0);

  // Choose a target DPI that strictly differs from the window's current DPI
  // (e.g. 192 vs 96) to simulate WM_DPICHANGED arriving before window rect
  // adjustment finishes.
  const UINT window_dpi = ::GetDpiForWindow(progress_wnd->hwnd());
  const UINT target_dpi = (window_dpi == 192) ? 96 : 192;
  ASSERT_NE(target_dpi, window_dpi);
  progress_wnd->ApplyDpiScaling(target_dpi);

  const HICON big_icon = reinterpret_cast<HICON>(
      ::SendMessage(progress_wnd->hwnd(), WM_GETICON, ICON_BIG, 0));
  const HICON small_icon = reinterpret_cast<HICON>(
      ::SendMessage(progress_wnd->hwnd(), WM_GETICON, ICON_SMALL, 0));
  ASSERT_NE(big_icon, nullptr);
  ASSERT_NE(small_icon, nullptr);

  ICONINFO big_info = {};
  ASSERT_TRUE(::GetIconInfo(big_icon, &big_info));
  base::win::ScopedGDIObject<HBITMAP> big_color(big_info.hbmColor);
  base::win::ScopedGDIObject<HBITMAP> big_mask(big_info.hbmMask);
  BITMAP bm_big = {};
  ASSERT_NE(::GetObject(big_color.get(), sizeof(bm_big), &bm_big), 0);

  ICONINFO small_info = {};
  ASSERT_TRUE(::GetIconInfo(small_icon, &small_info));
  base::win::ScopedGDIObject<HBITMAP> small_color(small_info.hbmColor);
  base::win::ScopedGDIObject<HBITMAP> small_mask(small_info.hbmMask);
  BITMAP bm_small = {};
  ASSERT_NE(::GetObject(small_color.get(), sizeof(bm_small), &bm_small), 0);

  const int expected_cx_big = ::GetSystemMetricsForDpi(SM_CXICON, target_dpi);
  const int expected_cy_big = ::GetSystemMetricsForDpi(SM_CYICON, target_dpi);
  const int expected_cx_small =
      ::GetSystemMetricsForDpi(SM_CXSMICON, target_dpi);
  const int expected_cy_small =
      ::GetSystemMetricsForDpi(SM_CYSMICON, target_dpi);

  const int stale_cx_big = ::GetSystemMetricsForDpi(SM_CXICON, window_dpi);
  const int stale_cx_small = ::GetSystemMetricsForDpi(SM_CXSMICON, window_dpi);

  // Assert icons match the target DPI, proving they were not overwritten by
  // a stale GetDpiForWindow() call.
  EXPECT_EQ(bm_big.bmWidth, expected_cx_big);
  EXPECT_EQ(std::abs(bm_big.bmHeight), expected_cy_big);
  EXPECT_NE(bm_big.bmWidth, stale_cx_big);

  EXPECT_EQ(bm_small.bmWidth, expected_cx_small);
  EXPECT_EQ(std::abs(bm_small.bmHeight), expected_cy_small);
  EXPECT_NE(bm_small.bmWidth, stale_cx_small);

  progress_wnd->DestroyWindow();
}

TEST_F(ProgressWndTest, SetCursorArrow) {
  MessageLoop ui_message_loop;
  ProgressWnd progress_wnd(&ui_message_loop, nullptr);
  progress_wnd.SetEventSink(this);
  progress_wnd.Initialize();
  progress_wnd.Show();

  // Send WM_SETCURSOR with HTCLIENT and verify it returns TRUE.
  LRESULT result = ::SendMessage(progress_wnd.hwnd(), WM_SETCURSOR,
                                 reinterpret_cast<WPARAM>(progress_wnd.hwnd()),
                                 MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
  EXPECT_EQ(result, static_cast<LRESULT>(TRUE));

  progress_wnd.DestroyWindow();
}

// Verifies that the error illustration is not loaded while the control is
// hidden, and switches between light and dark bitmap resources based on
// is_dark_mode() when visible.
TEST_F(ProgressWndTest, ErrorIllustrationThemeSwitching) {
  if (IsHighContrastOn()) {
    GTEST_SKIP();
  }
  registry_util::RegistryOverrideManager registry_override;
  ASSERT_NO_FATAL_FAILURE(
      registry_override.OverrideRegistry(HKEY_CURRENT_USER));

  EXPECT_CALL(*mock_progress_wnd_events_, DoExit())
      .Times(::testing::AnyNumber());

  MessageLoop ui_message_loop;

  // 1. In light mode, verify the error illustration is not loaded while hidden.
  SetDarkMode(false);
  {
    std::unique_ptr<ProgressWnd> progress_wnd =
        MakeProgressWindow(&ui_message_loop);
    const HWND error_ctl =
        ::GetDlgItem(progress_wnd->hwnd(), IDC_ERROR_ILLUSTRATION);
    ASSERT_NE(error_ctl, nullptr);
    EXPECT_FALSE(::IsWindowVisible(error_ctl));
    EXPECT_EQ(reinterpret_cast<HBITMAP>(
                  ::SendMessage(error_ctl, STM_GETIMAGE, IMAGE_BITMAP, 0)),
              nullptr);

    // Transition to error state: calling DisplayCompletionDialog(false, ...)
    // shows IDC_ERROR_ILLUSTRATION and triggers UpdateErrorIllustration().
    progress_wnd->DisplayCompletionDialog(false, L"Error message", "");
    EXPECT_TRUE(::IsWindowVisible(error_ctl));

    HBITMAP current_bmp = reinterpret_cast<HBITMAP>(
        ::SendMessage(error_ctl, STM_GETIMAGE, IMAGE_BITMAP, 0));
    EXPECT_EQ(current_bmp, progress_wnd->GetErrorIllustrationBitmap(false));
    EXPECT_NE(current_bmp, nullptr);

    // 2. Switch to dark mode via WM_SETTINGCHANGE and verify it updates to the
    // dark bitmap.
    SetDarkMode(true);
    ::SendMessage(progress_wnd->hwnd(), WM_SETTINGCHANGE, 0,
                  reinterpret_cast<LPARAM>(L"ImmersiveColorSet"));
    current_bmp = reinterpret_cast<HBITMAP>(
        ::SendMessage(error_ctl, STM_GETIMAGE, IMAGE_BITMAP, 0));
    EXPECT_EQ(current_bmp, progress_wnd->GetErrorIllustrationBitmap(true));
    EXPECT_NE(current_bmp, nullptr);
    EXPECT_NE(progress_wnd->GetErrorIllustrationBitmap(true),
              progress_wnd->GetErrorIllustrationBitmap(false));

    // 3. Switch back to light mode via WM_THEMECHANGED and verify it updates
    // back.
    SetDarkMode(false);
    ::SendMessage(progress_wnd->hwnd(), WM_THEMECHANGED, 0, 0);
    current_bmp = reinterpret_cast<HBITMAP>(
        ::SendMessage(error_ctl, STM_GETIMAGE, IMAGE_BITMAP, 0));
    EXPECT_EQ(current_bmp, progress_wnd->GetErrorIllustrationBitmap(false));

    progress_wnd->DestroyWindow();
  }

  // 4. Starting directly in dark mode should not load bitmap while hidden,
  // but set the dark bitmap when the error completion dialog is displayed.
  SetDarkMode(true);
  {
    std::unique_ptr<ProgressWnd> progress_wnd =
        MakeProgressWindow(&ui_message_loop);
    const HWND error_ctl =
        ::GetDlgItem(progress_wnd->hwnd(), IDC_ERROR_ILLUSTRATION);
    ASSERT_NE(error_ctl, nullptr);
    EXPECT_FALSE(::IsWindowVisible(error_ctl));
    EXPECT_EQ(reinterpret_cast<HBITMAP>(
                  ::SendMessage(error_ctl, STM_GETIMAGE, IMAGE_BITMAP, 0)),
              nullptr);

    progress_wnd->DisplayCompletionDialog(false, L"Error message", "");
    EXPECT_TRUE(::IsWindowVisible(error_ctl));

    HBITMAP current_bmp = reinterpret_cast<HBITMAP>(
        ::SendMessage(error_ctl, STM_GETIMAGE, IMAGE_BITMAP, 0));
    EXPECT_EQ(current_bmp, progress_wnd->GetErrorIllustrationBitmap(true));
    EXPECT_NE(current_bmp, nullptr);

    progress_wnd->DestroyWindow();
  }

  // 5. Calling DisplayCompletionDialog before Show() (parent dialog not yet
  // visible) still initializes the error illustration bitmap.
  SetDarkMode(false);
  {
    auto progress_wnd =
        std::make_unique<ProgressWnd>(&ui_message_loop, nullptr);
    progress_wnd->SetEventSink(this);
    progress_wnd->Initialize();
    // Intentionally do NOT call progress_wnd->Show() yet.
    EXPECT_FALSE(::IsWindowVisible(progress_wnd->hwnd()));

    const HWND error_ctl =
        ::GetDlgItem(progress_wnd->hwnd(), IDC_ERROR_ILLUSTRATION);
    ASSERT_NE(error_ctl, nullptr);
    EXPECT_FALSE(::IsWindowVisible(error_ctl));

    progress_wnd->DisplayCompletionDialog(false, L"Error message", "");
    EXPECT_TRUE(::GetWindowLongPtr(error_ctl, GWL_STYLE) & WS_VISIBLE);
    EXPECT_FALSE(::IsWindowVisible(error_ctl));

    HBITMAP current_bmp = reinterpret_cast<HBITMAP>(
        ::SendMessage(error_ctl, STM_GETIMAGE, IMAGE_BITMAP, 0));
    EXPECT_EQ(current_bmp, progress_wnd->GetErrorIllustrationBitmap(false));
    EXPECT_NE(current_bmp, nullptr);

    progress_wnd->DestroyWindow();
  }
}

TEST_F(ProgressWndTest, WindowIconsPersistAcrossCompletion) {
  using ::testing::AnyNumber;
  EXPECT_CALL(*mock_progress_wnd_events_, DoExit()).Times(AnyNumber());
  EXPECT_CALL(*mock_progress_wnd_events_, DoClose()).Times(AnyNumber());

  MessageLoop ui_message_loop;
  std::unique_ptr<ProgressWnd> progress_wnd =
      MakeProgressWindow(&ui_message_loop);

  const HICON initial_big_icon = reinterpret_cast<HICON>(
      ::SendMessage(progress_wnd->hwnd(), WM_GETICON, ICON_BIG, 0));
  const HICON initial_small_icon = reinterpret_cast<HICON>(
      ::SendMessage(progress_wnd->hwnd(), WM_GETICON, ICON_SMALL, 0));
  EXPECT_NE(initial_big_icon, nullptr);
  EXPECT_NE(initial_small_icon, nullptr);

  base::win::ScopedGetDC dc(progress_wnd->hwnd());
  base::win::ScopedGDIObject<HBITMAP> logo = CreateTestDIB24(dc, 48, 48);
  ASSERT_TRUE(logo.is_valid());

  ::SendMessage(progress_wnd->hwnd(), WM_SET_APP_LOGO,
                reinterpret_cast<WPARAM>(logo.release()), 0);

  const HICON custom_big_icon = reinterpret_cast<HICON>(
      ::SendMessage(progress_wnd->hwnd(), WM_GETICON, ICON_BIG, 0));
  const HICON custom_small_icon = reinterpret_cast<HICON>(
      ::SendMessage(progress_wnd->hwnd(), WM_GETICON, ICON_SMALL, 0));
  EXPECT_NE(custom_big_icon, nullptr);
  EXPECT_NE(custom_small_icon, nullptr);
  EXPECT_NE(custom_big_icon, initial_big_icon);
  EXPECT_NE(custom_small_icon, initial_small_icon);

  // Complete the installation and verify window icons remain active across
  // completion.
  ObserverCompletionInfo info;
  info.completion_code = CompletionCodes::COMPLETION_CODE_SUCCESS;
  info.completion_text = u"Install successful!";
  AppCompletionInfo app_info;
  app_info.completion_code = CompletionCodes::COMPLETION_CODE_SUCCESS;
  info.apps_info.push_back(app_info);
  progress_wnd->OnComplete(info);

  const HICON completed_big_icon = reinterpret_cast<HICON>(
      ::SendMessage(progress_wnd->hwnd(), WM_GETICON, ICON_BIG, 0));
  const HICON completed_small_icon = reinterpret_cast<HICON>(
      ::SendMessage(progress_wnd->hwnd(), WM_GETICON, ICON_SMALL, 0));
  EXPECT_EQ(completed_big_icon, custom_big_icon);
  EXPECT_EQ(completed_small_icon, custom_small_icon);

  const HWND app_bitmap_ctl =
      ::GetDlgItem(progress_wnd->hwnd(), IDC_APP_BITMAP);
  EXPECT_TRUE(::IsWindowVisible(app_bitmap_ctl));

  progress_wnd->DestroyWindow();
}

TEST_F(ProgressWndTest, WindowIconOnLogoAndDpiChange) {
  MessageLoop ui_message_loop;
  std::unique_ptr<ProgressWnd> progress_wnd =
      MakeProgressWindow(&ui_message_loop);

  base::win::ScopedGetDC dc(progress_wnd->hwnd());
  base::win::ScopedGDIObject<HBITMAP> logo1 = CreateTestDIB24(dc, 48, 48);
  ASSERT_TRUE(logo1.is_valid());
  ::SendMessage(progress_wnd->hwnd(), WM_SET_APP_LOGO,
                reinterpret_cast<WPARAM>(logo1.release()), 0);

  const HICON logo1_big_icon = reinterpret_cast<HICON>(
      ::SendMessage(progress_wnd->hwnd(), WM_GETICON, ICON_BIG, 0));
  EXPECT_NE(logo1_big_icon, nullptr);

  // Update logo a second time: exercises updating icons on the same DPI.
  base::win::ScopedGDIObject<HBITMAP> logo2 = CreateTestDIB24(dc, 48, 48);
  ASSERT_TRUE(logo2.is_valid());
  ::SendMessage(progress_wnd->hwnd(), WM_SET_APP_LOGO,
                reinterpret_cast<WPARAM>(logo2.release()), 0);

  const HICON logo2_big_icon = reinterpret_cast<HICON>(
      ::SendMessage(progress_wnd->hwnd(), WM_GETICON, ICON_BIG, 0));
  EXPECT_NE(logo2_big_icon, nullptr);

  // Trigger DPI change: exercises loading icons for new DPI.
  RECT new_rect = {0, 0, 800, 600};
  ::SendMessage(progress_wnd->hwnd(), WM_DPICHANGED, MAKELONG(144, 144),
                reinterpret_cast<LPARAM>(&new_rect));

  const HICON dpi_big_icon = reinterpret_cast<HICON>(
      ::SendMessage(progress_wnd->hwnd(), WM_GETICON, ICON_BIG, 0));
  EXPECT_NE(dpi_big_icon, nullptr);

  // Trigger DPI change back to original: exercises updating icons for
  // previously visited DPI.
  const UINT original_dpi = ::GetDpiForWindow(progress_wnd->hwnd());
  ::SendMessage(progress_wnd->hwnd(), WM_DPICHANGED,
                MAKELONG(original_dpi, original_dpi),
                reinterpret_cast<LPARAM>(&new_rect));

  const HICON restored_big_icon = reinterpret_cast<HICON>(
      ::SendMessage(progress_wnd->hwnd(), WM_GETICON, ICON_BIG, 0));
  EXPECT_NE(restored_big_icon, nullptr);

  // Clear logo: exercises fallback to default icon (IDI_APP).
  ::SendMessage(progress_wnd->hwnd(), WM_SET_APP_LOGO, 0, 0);
  const HICON default_icon = reinterpret_cast<HICON>(
      ::SendMessage(progress_wnd->hwnd(), WM_GETICON, ICON_BIG, 0));
  EXPECT_NE(default_icon, nullptr);

  progress_wnd->DestroyWindow();
}

TEST_F(ProgressWndTest, SetAppLogoHybridTheme) {
  if (IsHighContrastOn()) {
    GTEST_SKIP();
  }
  registry_util::RegistryOverrideManager registry_override;
  ASSERT_NO_FATAL_FAILURE(
      registry_override.OverrideRegistry(HKEY_CURRENT_USER));

  auto set_theme_mode = [](bool app_dark, bool system_dark) {
    base::win::RegKey key;
    EXPECT_EQ(key.Create(HKEY_CURRENT_USER,
                         L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes"
                         L"\\Personalize",
                         KEY_SET_VALUE),
              ERROR_SUCCESS);
    EXPECT_EQ(key.WriteValue(L"AppsUseLightTheme",
                             static_cast<DWORD>(app_dark ? 0 : 1)),
              ERROR_SUCCESS);
    EXPECT_EQ(key.WriteValue(L"SystemUsesLightTheme",
                             static_cast<DWORD>(system_dark ? 0 : 1)),
              ERROR_SUCCESS);
  };

  // Configure Hybrid Mode: App UI is Dark, System/Taskbar is Light.
  set_theme_mode(/*app_dark=*/true, /*system_dark=*/false);

  MessageLoop ui_message_loop;
  std::unique_ptr<ProgressWnd> progress_wnd =
      MakeProgressWindow(&ui_message_loop);

  base::win::ScopedGetDC dc(nullptr);
  base::win::ScopedGDIObject<HBITMAP> light_bitmap =
      CreateTestDIB24(dc, 32, 32);
  base::win::ScopedGDIObject<HBITMAP> dark_bitmap = CreateTestDIB24(dc, 48, 48);
  ASSERT_TRUE(light_bitmap.is_valid());
  ASSERT_TRUE(dark_bitmap.is_valid());

  const HBITMAP light_hbitmap = light_bitmap.get();
  const HBITMAP dark_hbitmap = dark_bitmap.get();

  ::SendMessage(progress_wnd->hwnd(), WM_SET_APP_LOGO,
                reinterpret_cast<WPARAM>(light_bitmap.release()),
                reinterpret_cast<LPARAM>(dark_bitmap.release()));

  // In dark app mode, dialog app logo uses dark bitmap.
  EXPECT_EQ(progress_wnd->GetCurrentAppLogoBitmap(), dark_hbitmap);

  // In hybrid mode, big icon (taskbar) uses light bitmap, while small icon
  // (titlebar) uses dark bitmap.
  EXPECT_EQ(progress_wnd->current_logo_big_for_testing(), light_hbitmap);
  EXPECT_EQ(progress_wnd->current_logo_small_for_testing(), dark_hbitmap);

  const HICON hybrid_big_icon = reinterpret_cast<HICON>(
      ::SendMessage(progress_wnd->hwnd(), WM_GETICON, ICON_BIG, 0));
  const HICON hybrid_small_icon = reinterpret_cast<HICON>(
      ::SendMessage(progress_wnd->hwnd(), WM_GETICON, ICON_SMALL, 0));
  EXPECT_NE(hybrid_big_icon, nullptr);
  EXPECT_NE(hybrid_small_icon, nullptr);

  // Switch to full dark mode (both app and system dark).
  set_theme_mode(/*app_dark=*/true, /*system_dark=*/true);
  ::SendMessage(progress_wnd->hwnd(), WM_SETTINGCHANGE, 0,
                reinterpret_cast<LPARAM>(L"ImmersiveColorSet"));

  EXPECT_EQ(progress_wnd->GetCurrentAppLogoBitmap(), dark_hbitmap);
  EXPECT_EQ(progress_wnd->current_logo_big_for_testing(), dark_hbitmap);
  EXPECT_EQ(progress_wnd->current_logo_small_for_testing(), dark_hbitmap);

  // Switch to hybrid mode with app light, system dark.
  set_theme_mode(/*app_dark=*/false, /*system_dark=*/true);
  ::SendMessage(progress_wnd->hwnd(), WM_SYSCOLORCHANGE, 0, 0);

  EXPECT_EQ(progress_wnd->GetCurrentAppLogoBitmap(), light_hbitmap);
  EXPECT_EQ(progress_wnd->current_logo_big_for_testing(), dark_hbitmap);
  EXPECT_EQ(progress_wnd->current_logo_small_for_testing(), light_hbitmap);

  progress_wnd->DestroyWindow();
}

TEST_F(ProgressWndTest, SetAppLogoIdenticalBitmapHandles) {
  MessageLoop ui_message_loop;
  std::unique_ptr<ProgressWnd> progress_wnd =
      MakeProgressWindow(&ui_message_loop);

  base::win::ScopedGetDC dc(nullptr);
  base::win::ScopedGDIObject<HBITMAP> logo1 = CreateTestDIB24(dc, 48, 48);
  ASSERT_TRUE(logo1.is_valid());
  const HBITMAP hbitmap1 = logo1.get();

  // Test 1: Sending WM_SET_APP_LOGO with identical WPARAM and LPARAM normalizes
  // at the message boundary: ownership of hbitmap1 is transferred to
  // light_app_logo_bmp_ while dark_app_logo_bmp_ is set to null, avoiding
  // duplicate ScopedGDIObject wrappers for the same physical handle while
  // allowing SelectLogoForTheme() to fall back to light.
  ::SendMessage(progress_wnd->hwnd(), WM_SET_APP_LOGO,
                reinterpret_cast<WPARAM>(logo1.release()),
                reinterpret_cast<LPARAM>(hbitmap1));
  EXPECT_EQ(progress_wnd->GetCurrentAppLogoBitmap(), hbitmap1);
  EXPECT_EQ(progress_wnd->current_logo_big_for_testing(), hbitmap1);
  EXPECT_EQ(progress_wnd->current_logo_small_for_testing(), hbitmap1);
  EXPECT_EQ(progress_wnd->light_app_logo_bmp_.get(), hbitmap1);
  EXPECT_EQ(progress_wnd->dark_app_logo_bmp_.get(), nullptr);

  // Test 2: Sending WM_SET_APP_LOGO with distinct handles assigns each theme
  // its own logo.
  base::win::ScopedGDIObject<HBITMAP> logo2 = CreateTestDIB24(dc, 32, 32);
  base::win::ScopedGDIObject<HBITMAP> logo3 = CreateTestDIB24(dc, 32, 32);
  ASSERT_TRUE(logo2.is_valid());
  ASSERT_TRUE(logo3.is_valid());
  const HBITMAP hbitmap2 = logo2.get();
  const HBITMAP hbitmap3 = logo3.get();

  ::SendMessage(progress_wnd->hwnd(), WM_SET_APP_LOGO,
                reinterpret_cast<WPARAM>(logo2.release()),
                reinterpret_cast<LPARAM>(logo3.release()));
  EXPECT_EQ(progress_wnd->light_app_logo_bmp_.get(), hbitmap2);
  EXPECT_EQ(progress_wnd->dark_app_logo_bmp_.get(), hbitmap3);

  // Test 3: Calling SetAppLogo directly with a single logo transfers ownership
  // to light_app_logo_bmp_ with dark_app_logo_bmp_ null (the canonical way
  // to use a single logo across themes).
  base::win::ScopedGDIObject<HBITMAP> logo4 = CreateTestDIB24(dc, 48, 48);
  ASSERT_TRUE(logo4.is_valid());
  const HBITMAP hbitmap4 = logo4.get();
  progress_wnd->SetAppLogo(std::move(logo4));
  EXPECT_EQ(progress_wnd->GetCurrentAppLogoBitmap(), hbitmap4);
  EXPECT_EQ(progress_wnd->light_app_logo_bmp_.get(), hbitmap4);
  EXPECT_EQ(progress_wnd->dark_app_logo_bmp_.get(), nullptr);

  // Test 4: Calling SetAppLogo with two distinct logos.
  base::win::ScopedGDIObject<HBITMAP> new_light = CreateTestDIB24(dc, 32, 32);
  base::win::ScopedGDIObject<HBITMAP> new_dark = CreateTestDIB24(dc, 32, 32);
  const HBITMAP hbitmap_new_light = new_light.get();
  const HBITMAP hbitmap_new_dark = new_dark.get();
  progress_wnd->SetAppLogo(std::move(new_light), std::move(new_dark));
  EXPECT_EQ(progress_wnd->light_app_logo_bmp_.get(), hbitmap_new_light);
  EXPECT_EQ(progress_wnd->dark_app_logo_bmp_.get(), hbitmap_new_dark);

  // Test 5: Clearing logos via null WPARAM and LPARAM.
  ::SendMessage(progress_wnd->hwnd(), WM_SET_APP_LOGO, 0, 0);
  EXPECT_EQ(progress_wnd->GetCurrentAppLogoBitmap(), nullptr);
  EXPECT_EQ(progress_wnd->light_app_logo_bmp_.get(), nullptr);
  EXPECT_EQ(progress_wnd->dark_app_logo_bmp_.get(), nullptr);

  progress_wnd->DestroyWindow();
}

TEST_F(ProgressWndTest,
       ResetWindowIconCachePreventsGdiHandleRecyclingStaleCacheHit) {
  MessageLoop ui_message_loop;
  std::unique_ptr<ProgressWnd> progress_wnd =
      MakeProgressWindow(&ui_message_loop);

  base::win::ScopedGetDC dc(nullptr);
  base::win::ScopedGDIObject<HBITMAP> logo1 = CreateTestDIB24(dc, 48, 48);
  ASSERT_TRUE(logo1.is_valid());
  const HBITMAP hbitmap1 = logo1.get();

  // Set initial app logo and verify window icon cache holds the handle.
  progress_wnd->SetAppLogo(std::move(logo1));
  EXPECT_EQ(progress_wnd->current_logo_big_for_testing(), hbitmap1);
  EXPECT_EQ(progress_wnd->current_logo_small_for_testing(), hbitmap1);

  // Calling ResetWindowIconCache clears the cached logo handles.
  progress_wnd->ResetWindowIconCache();
  EXPECT_EQ(progress_wnd->current_logo_big_for_testing(), nullptr);
  EXPECT_EQ(progress_wnd->current_logo_small_for_testing(), nullptr);

  // Re-invoking SetAppLogo automatically resets the cache, allowing a
  // recycled GDI handle address to trigger fresh window icon generation
  // instead of a false cache hit.
  base::win::ScopedGDIObject<HBITMAP> logo2 = CreateTestDIB24(dc, 48, 48);
  ASSERT_TRUE(logo2.is_valid());
  const HBITMAP hbitmap2 = logo2.get();
  progress_wnd->SetAppLogo(std::move(logo2));
  EXPECT_EQ(progress_wnd->current_logo_big_for_testing(), hbitmap2);
  EXPECT_EQ(progress_wnd->current_logo_small_for_testing(), hbitmap2);

  progress_wnd->DestroyWindow();
}

TEST_F(ProgressWndTest,
       GetCurrentAppLogoBitmapDarkModeSingleLightBitmapFallback) {
  if (IsHighContrastOn()) {
    GTEST_SKIP();
  }
  registry_util::RegistryOverrideManager registry_override;
  ASSERT_NO_FATAL_FAILURE(
      registry_override.OverrideRegistry(HKEY_CURRENT_USER));

  auto set_dark_mode = [](bool dark) {
    base::win::RegKey key;
    EXPECT_EQ(key.Create(HKEY_CURRENT_USER,
                         L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes"
                         L"\\Personalize",
                         KEY_SET_VALUE),
              ERROR_SUCCESS);
    EXPECT_EQ(
        key.WriteValue(L"AppsUseLightTheme", static_cast<DWORD>(dark ? 0 : 1)),
        ERROR_SUCCESS);
    EXPECT_EQ(key.WriteValue(L"SystemUsesLightTheme",
                             static_cast<DWORD>(dark ? 0 : 1)),
              ERROR_SUCCESS);
  };

  // Case 1: Window is initialized in dark mode, and SetAppLogo is called with
  // only a single light logo bitmap.
  set_dark_mode(true);

  MessageLoop ui_message_loop;
  std::unique_ptr<ProgressWnd> progress_wnd =
      MakeProgressWindow(&ui_message_loop);
  ASSERT_TRUE(progress_wnd->is_dark_mode());

  base::win::ScopedGetDC dc(nullptr);
  base::win::ScopedGDIObject<HBITMAP> light_logo = CreateTestDIB24(dc, 48, 48);
  ASSERT_TRUE(light_logo.is_valid());
  const HBITMAP light_hbitmap = light_logo.get();

  // Call SetAppLogo with a single light bitmap parameter by value (dark_bitmap
  // defaults to an empty ScopedGDIObject).
  progress_wnd->SetAppLogo(std::move(light_logo));

  EXPECT_EQ(progress_wnd->light_app_logo_bmp_.get(), light_hbitmap);
  EXPECT_EQ(progress_wnd->dark_app_logo_bmp_.get(), nullptr);

  // In dark mode with only a single light logo provided,
  // GetCurrentAppLogoBitmap() must fall back to the light logo handle instead
  // of returning nullptr.
  EXPECT_EQ(progress_wnd->GetCurrentAppLogoBitmap(), light_hbitmap);

  // Verify that UpdateAppLogo did not wipe the logo or fall back to IDI_APP.
  EXPECT_EQ(progress_wnd->current_logo_big_for_testing(), light_hbitmap);
  EXPECT_EQ(progress_wnd->current_logo_small_for_testing(), light_hbitmap);

  const HWND app_bitmap_ctl =
      ::GetDlgItem(progress_wnd->hwnd(), IDC_APP_BITMAP);
  ASSERT_NE(app_bitmap_ctl, nullptr);
  EXPECT_NE(reinterpret_cast<HBITMAP>(
                ::SendMessage(app_bitmap_ctl, STM_GETIMAGE, IMAGE_BITMAP, 0)),
            nullptr);

  // Case 2: Dynamically switch to light mode and back to dark mode, verifying
  // the fallback continues to return the light logo without clearing it.
  set_dark_mode(false);
  ::SendMessage(progress_wnd->hwnd(), WM_SYSCOLORCHANGE, 0, 0);
  EXPECT_FALSE(progress_wnd->is_dark_mode());
  EXPECT_EQ(progress_wnd->GetCurrentAppLogoBitmap(), light_hbitmap);

  set_dark_mode(true);
  ::SendMessage(progress_wnd->hwnd(), WM_SETTINGCHANGE, 0,
                reinterpret_cast<LPARAM>(L"ImmersiveColorSet"));
  EXPECT_TRUE(progress_wnd->is_dark_mode());
  EXPECT_EQ(progress_wnd->GetCurrentAppLogoBitmap(), light_hbitmap);
  EXPECT_EQ(progress_wnd->current_logo_big_for_testing(), light_hbitmap);
  EXPECT_EQ(progress_wnd->current_logo_small_for_testing(), light_hbitmap);

  progress_wnd->DestroyWindow();
}

}  // namespace updater::ui
