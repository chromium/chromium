// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/ui/progress_wnd.h"

#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "chrome/updater/test/test_scope.h"
#include "chrome/updater/test/unit_test_util.h"
#include "chrome/updater/test/unit_test_util_win.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/test/test_executables.h"
#include "chrome/updater/win/test/test_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/wtl/include/atlapp.h"

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

  std::unique_ptr<ProgressWnd> MakeProgressWindow(
      WTL::CMessageLoop* message_loop) {
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
    WTL::CMessageLoop ui_message_loop;
    std::unique_ptr<ProgressWnd> progress_wnd =
        MakeProgressWindow(&ui_message_loop);
    progress_wnd->OnComplete(observer_completion_info);
    const HWND button = progress_wnd->GetDlgItem(button_to_push);
    progress_wnd->SendMessage(WM_COMMAND,
                              MAKEWPARAM(button_to_push, BN_CLICKED),
                              reinterpret_cast<LPARAM>(button));
  };
  {
    mock_progress_wnd_events_ = std::make_unique<MockProgressWndEvents>();
    ::testing::InSequence seq;
    EXPECT_CALL(*mock_progress_wnd_events_,
                DoRestartBrowser(
                    false, std::vector<GURL>{GURL("http://some-test-url")}))
        .Times(1)
        .WillOnce(::testing::Return(true));
    EXPECT_CALL(*mock_progress_wnd_events_, DoExit()).Times(1);
    EXPECT_CALL(*mock_progress_wnd_events_, DoClose()).Times(1);
    button_tester(CompletionCodes::COMPLETION_CODE_RESTART_BROWSER,
                  IDC_BUTTON1);
  }
  {
    mock_progress_wnd_events_ = std::make_unique<MockProgressWndEvents>();
    ::testing::InSequence seq;
    EXPECT_CALL(
        *mock_progress_wnd_events_,
        DoRestartBrowser(true, std::vector<GURL>{GURL("http://some-test-url")}))
        .Times(1)
        .WillOnce(::testing::Return(true));
    EXPECT_CALL(*mock_progress_wnd_events_, DoExit()).Times(1);
    EXPECT_CALL(*mock_progress_wnd_events_, DoClose()).Times(1);
    button_tester(CompletionCodes::COMPLETION_CODE_RESTART_ALL_BROWSERS,
                  IDC_BUTTON1);
  }
  {
    mock_progress_wnd_events_ = std::make_unique<MockProgressWndEvents>();
    ::testing::InSequence seq;
    EXPECT_CALL(*mock_progress_wnd_events_, DoReboot())
        .Times(1)
        .WillOnce(::testing::Return(true));
    EXPECT_CALL(*mock_progress_wnd_events_, DoExit()).Times(1);
    EXPECT_CALL(*mock_progress_wnd_events_, DoClose()).Times(1);
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
    EXPECT_CALL(*mock_progress_wnd_events_, DoExit()).Times(1);
    EXPECT_CALL(*mock_progress_wnd_events_, DoClose()).Times(1);
    button_tester(completion_code, IDC_BUTTON2);
  }

  for (auto completion_code : {CompletionCodes::COMPLETION_CODE_SUCCESS,
                               CompletionCodes::COMPLETION_CODE_ERROR}) {
    mock_progress_wnd_events_ = std::make_unique<MockProgressWndEvents>();
    ::testing::InSequence seq;
    EXPECT_CALL(*mock_progress_wnd_events_,
                DoRestartBrowser(::testing::_, ::testing::_))
        .Times(0);
    EXPECT_CALL(*mock_progress_wnd_events_, DoExit()).Times(1);
    EXPECT_CALL(*mock_progress_wnd_events_, DoClose()).Times(1);
    button_tester(completion_code, IDC_CLOSE);
  }
}

TEST_F(ProgressWndTest, OnInstallStopped) {
  for (auto id : {IDOK, IDCANCEL}) {
    mock_progress_wnd_events_ = std::make_unique<MockProgressWndEvents>();
    WTL::CMessageLoop ui_message_loop;
    std::unique_ptr<ProgressWnd> progress_wnd =
        MakeProgressWindow(&ui_message_loop);
    BOOL handled = false;
    if (id == IDCANCEL) {
      EXPECT_CALL(*mock_progress_wnd_events_, DoCancel()).Times(1);
    }
    progress_wnd->OnInstallStopped(WM_INSTALL_STOPPED, id, 0, handled);
    if (id == IDCANCEL) {
      EXPECT_TRUE(progress_wnd->is_canceled_);

      // Second call to `OnInstallStopped` is ignored.
      progress_wnd->OnInstallStopped(WM_INSTALL_STOPPED, id, 0, handled);
    }
    EXPECT_TRUE(handled);
    progress_wnd->DestroyWindow();
  }
}

TEST_F(ProgressWndTest, MaybeCloseWindow) {
  mock_progress_wnd_events_ = std::make_unique<MockProgressWndEvents>();
  EXPECT_CALL(*mock_progress_wnd_events_, DoCancel()).WillOnce([] {
    ::PostThreadMessage(::GetCurrentThreadId(), WM_QUIT, 0, 0);
  });
  WTL::CMessageLoop message_loop;
  std::unique_ptr<ProgressWnd> progress_wnd = MakeProgressWindow(&message_loop);
  progress_wnd->MaybeCloseWindow();
  EXPECT_TRUE(progress_wnd->IsInstallStoppedWindowPresent());
  const HWND button = progress_wnd->install_stopped_wnd_->GetDlgItem(IDOK);
  progress_wnd->install_stopped_wnd_->SendMessage(
      WM_COMMAND, MAKEWPARAM(IDCANCEL, BN_CLICKED),
      reinterpret_cast<LPARAM>(button));
  message_loop.Run();
  EXPECT_FALSE(progress_wnd->IsInstallStoppedWindowPresent());
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
    WTL::CMessageLoop message_loop;
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
  WTL::CMessageLoop ui_message_loop;
  std::unique_ptr<ProgressWnd> progress_wnd =
      MakeProgressWindow(&ui_message_loop);
  progress_wnd->OnCheckingForUpdate();
  EXPECT_EQ(progress_wnd->cur_state_,
            ProgressWnd::States::STATE_CHECKING_FOR_UPDATE);
  EXPECT_FALSE(::IsWindowEnabled(progress_wnd->GetDlgItem(IDC_CLOSE)));
  progress_wnd->DestroyWindow();
}

TEST_F(ProgressWndTest, OnWaitingToDownload) {
  for (const int is_retry : {false, true}) {
    WTL::CMessageLoop ui_message_loop;
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
    EXPECT_FALSE(::IsWindowEnabled(progress_wnd->GetDlgItem(IDC_CLOSE)));
    std::wstring state_text(kMaxStringLen, 0);
    progress_wnd->GetDlgItemText(IDC_INSTALLER_STATE_TEXT, state_text.data(),
                                 kMaxStringLen);
    EXPECT_STREQ(state_text.c_str(), L"");
    progress_wnd->DestroyWindow();
  }
}

TEST_F(ProgressWndTest, OnPause) {
  WTL::CMessageLoop ui_message_loop;
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

  WTL::CMessageLoop ui_message_loop;
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
    std::wstring completion_text(kMaxStringLen, 0);
    progress_wnd->GetDlgItemText(IDC_COMPLETE_TEXT, completion_text.data(),
                                 kMaxStringLen);
    EXPECT_STREQ(completion_text.c_str(), L"text");
    EXPECT_TRUE(::IsWindowEnabled(progress_wnd->GetDlgItem(IDC_CLOSE)));
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
  WTL::CMessageLoop ui_message_loop;
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

}  // namespace updater::ui
