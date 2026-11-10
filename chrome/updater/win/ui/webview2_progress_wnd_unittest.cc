// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/ui/webview2_progress_wnd.h"

#include <windows.h>

#include <shlobj.h>

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/base_paths_win.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/scoped_environment_variable_override.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/updater/app/app_install_progress.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/win/ui/l10n_util.h"
#include "chrome/updater/win/ui/message_loop.h"
#include "chrome/updater/win/ui/resources/updater_installer_strings.h"
#include "chrome/updater/win/ui/ui_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

namespace updater::ui {
namespace {
constexpr base::TimeDelta kHangGuard = base::Seconds(5);

// Exercises real HWND dispatch and timers, but never starts WebView2 processes.
class RecoveryTestWindow : public WebView2ProgressWnd {
 public:
  ~RecoveryTestWindow() override { Close(); }

  void Close() {
    if (::IsWindow(hwnd())) {
      ::DestroyWindow(hwnd());
      // Normal WM_DESTROY posts quit. Do not leave that message on a pooled
      // STA worker after a test that doesn't run a native MessageLoop.
      MSG msg = {};
      while (::PeekMessage(&msg, nullptr, WM_QUIT, WM_QUIT, PM_REMOVE)) {
      }
    }
  }

  int RunNativeLoop() {
    if (!::SetTimer(hwnd(), kGuardTimerId,
                    static_cast<UINT>(kHangGuard.InMilliseconds()), nullptr)) {
      ADD_FAILURE() << "Could not start test hang guard";
      return 1;
    }
    MessageLoop loop;
    const int result = loop.Run();
    ::KillTimer(hwnd(), kGuardTimerId);
    return result;
  }

  void CompleteScript(const std::wstring& result) {
    ASSERT_TRUE(completion);
    std::move(completion).Run(result);
  }

  BOOL ProcessWindowMessage(HWND hwnd,
                            UINT message,
                            WPARAM wparam,
                            LPARAM lparam,
                            LRESULT& result,
                            DWORD map_id = 0) override {
    if (message == WM_TIMER && wparam == kGuardTimerId) {
      ::PostQuitMessage(1);
      result = 0;
      return TRUE;
    }
    const BOOL handled = WebView2ProgressWnd::ProcessWindowMessage(
        hwnd, message, wparam, lparam, result, map_id);
    if (message == WM_DESTROY) {
      destroy_handled = handled;
    }
    return handled;
  }

  bool destroy_handled = true;
  HRESULT create_result = S_OK;
  int creates = 0;
  std::vector<std::wstring> scripts;
  base::OnceCallback<void(const std::wstring&)> completion;

 protected:
  HRESULT CreateBrowser() override {
    ++creates;
    return create_result;
  }
  HRESULT ExecuteUIUpdate(
      const std::wstring& script,
      base::OnceCallback<void(const std::wstring&)> on_complete) override {
    EXPECT_FALSE(completion);  // At most one update may be in flight.
    scripts.push_back(script);
    completion = std::move(on_complete);
    return S_OK;
  }

 private:
  static constexpr UINT_PTR kGuardTimerId = 0x7fff;
};

}  // namespace

class WebView2ProgressWndTest : public ::testing::Test {
 protected:
  // The runner's main thread is COM MTA. Exercise and destroy windows on STA.
  // Process command-line overrides must stay on the main thread; this helper
  // waits for the STA task before returning and restoring any override.
  void RunOnSTAThread(base::OnceClosure check) {
    base::test::TestFuture<void> done;
    base::ThreadPool::CreateCOMSTATaskRunner({base::MayBlock()})
        ->PostTaskAndReply(FROM_HERE, std::move(check), done.GetCallback());
    EXPECT_TRUE(done.Wait());
  }

  base::test::TaskEnvironment task_environment_;
};

TEST_F(WebView2ProgressWndTest, InitializationTimeoutFallsBack) {
  RunOnSTAThread(base::BindOnce([] {
    RecoveryTestWindow wnd;
    int fallbacks = 0;
    wnd.set_creation_failed_callback(base::BindOnce(
        [](int* count) {
          ++*count;
          ::PostQuitMessage(0);
        },
        &fallbacks));
    wnd.Init(nullptr, gfx::Rect(0, 0, 100, 100));
    ASSERT_EQ(wnd.creates, 1);
    ASSERT_TRUE(
        ::SetTimer(wnd.hwnd(), wnd.kInitTimerId, USER_TIMER_MINIMUM, nullptr));
    EXPECT_EQ(wnd.RunNativeLoop(), 0);
    EXPECT_EQ(fallbacks, 1);
  }));
}

TEST_F(WebView2ProgressWndTest, CreationFailureIsDeferred) {
  RunOnSTAThread(base::BindOnce([] {
    RecoveryTestWindow wnd;
    wnd.create_result = E_FAIL;
    int fallbacks = 0;
    wnd.set_creation_failed_callback(base::BindOnce(
        [](int* count) {
          ++*count;
          ::PostQuitMessage(0);
        },
        &fallbacks));
    wnd.Init(nullptr, gfx::Rect(0, 0, 100, 100));
    EXPECT_EQ(fallbacks, 0);
    EXPECT_EQ(wnd.RunNativeLoop(), 0);
    EXPECT_EQ(fallbacks, 1);
  }));
}

TEST_F(WebView2ProgressWndTest, FailedUpdateFallsBackOnce) {
  RunOnSTAThread(base::BindOnce([] {
    RecoveryTestWindow wnd;
    int fallbacks = 0;
    wnd.set_creation_failed_callback(base::BindOnce(
        [](int* count) {
          ++*count;
          ::PostQuitMessage(0);
        },
        &fallbacks));
    wnd.Init(nullptr, gfx::Rect(0, 0, 100, 100));
    wnd.OnWebMessageReceived(L"ui_ready");
    ASSERT_EQ(wnd.scripts.size(), 1u);
    wnd.CompleteScript(L"null");
    wnd.ScheduleFallback(E_ABORT);  // Duplicate notification.
    EXPECT_EQ(fallbacks, 0);  // Must leave the callback before handing off.
    EXPECT_EQ(wnd.RunNativeLoop(), 0);
    EXPECT_EQ(fallbacks, 1);
    wnd.ScheduleFallback(E_FAIL);
    wnd.OnWebViewCreated(E_FAIL);  // Late creation callback must be ignored.
    EXPECT_EQ(fallbacks, 1);
    wnd.Close();
    EXPECT_FALSE(wnd.destroy_handled);
  }));
}

TEST_F(WebView2ProgressWndTest, UpdatesAreCoalesced) {
  RunOnSTAThread(base::BindOnce([] {
    RecoveryTestWindow wnd;
    int fallbacks = 0;
    wnd.set_creation_failed_callback(
        base::BindOnce([](int* count) { ++*count; }, &fallbacks));
    wnd.Init(nullptr, gfx::Rect(0, 0, 100, 100));
    wnd.OnWebMessageReceived(L"ui_ready");
    wnd.OnDownloading("app", u"First", std::nullopt, 20);
    wnd.OnInstalling("app", u"Latest", std::nullopt, 88);
    EXPECT_EQ(wnd.scripts.size(), 1u);
    wnd.CompleteScript(L"true");
    ASSERT_EQ(wnd.scripts.size(), 2u);
    EXPECT_NE(wnd.scripts.back().find(L"\"progress\":88"), std::wstring::npos);
    EXPECT_NE(wnd.scripts.back().find(L"Latest"), std::wstring::npos);
    wnd.CompleteScript(L"true");
    // A stale initialization timer cannot cause fallback after rendering.
    ::SendMessage(wnd.hwnd(), WM_TIMER, wnd.kInitTimerId, 0);
    ::SendMessage(wnd.hwnd(), WM_TIMER, wnd.kUpdateTimerId, 0);
    wnd.OnWebMessageReceived(L"ui_ready");  // Duplicate readiness.
    EXPECT_EQ(wnd.scripts.size(), 2u);
    EXPECT_EQ(fallbacks, 0);
  }));
}

TEST_F(WebView2ProgressWndTest, ProgressTimeoutIsNotPostponed) {
  RunOnSTAThread(base::BindOnce([] {
    RecoveryTestWindow wnd;
    int fallbacks = 0;
    wnd.set_creation_failed_callback(base::BindOnce(
        [](int* count) {
          ++*count;
          ::PostQuitMessage(0);
        },
        &fallbacks));
    wnd.Init(nullptr, gfx::Rect(0, 0, 100, 100));
    wnd.OnWebMessageReceived(L"ui_ready");
    wnd.CompleteScript(L"true");
    wnd.OnDownloading("app", u"App", std::nullopt, 10);
    // Shorten the outstanding timer. Coalescing another update must not
    // rearm it with the production timeout (longer than the hang guard).
    ASSERT_TRUE(::SetTimer(wnd.hwnd(), wnd.kUpdateTimerId, USER_TIMER_MINIMUM,
                           nullptr));
    wnd.OnDownloading("app", u"App", std::nullopt, 20);
    EXPECT_EQ(wnd.scripts.size(), 2u);
    EXPECT_EQ(wnd.RunNativeLoop(), 0);
    EXPECT_EQ(fallbacks, 1);
    wnd.CompleteScript(L"true");  // Late completion after handoff.
    EXPECT_EQ(wnd.scripts.size(), 2u);
    EXPECT_EQ(fallbacks, 1);
  }));
}

TEST_F(WebView2ProgressWndTest, PageFailureFallsBack) {
  RunOnSTAThread(base::BindOnce([] {
    RecoveryTestWindow wnd;
    int fallbacks = 0;
    wnd.set_creation_failed_callback(base::BindOnce(
        [](int* count) {
          ++*count;
          ::PostQuitMessage(0);
        },
        &fallbacks));
    wnd.Init(nullptr, gfx::Rect(0, 0, 100, 100));
    wnd.OnWebMessageReceived(L"ui_failed");
    EXPECT_EQ(fallbacks, 0);
    EXPECT_EQ(wnd.RunNativeLoop(), 0);
    EXPECT_EQ(fallbacks, 1);
  }));
}

TEST_F(WebView2ProgressWndTest, ClosingCancelsRecovery) {
  RunOnSTAThread(base::BindOnce([] {
    RecoveryTestWindow wnd;
    int fallbacks = 0;
    wnd.set_creation_failed_callback(
        base::BindOnce([](int* count) { ++*count; }, &fallbacks));
    wnd.Init(nullptr, gfx::Rect(0, 0, 100, 100));
    wnd.OnWebMessageReceived(L"ui_ready");
    wnd.Close();
    EXPECT_FALSE(wnd.destroy_handled);
    wnd.CompleteScript(L"");
    wnd.ScheduleFallback(E_FAIL);
    EXPECT_EQ(fallbacks, 0);
  }));
}

TEST_F(WebView2ProgressWndTest, CancellationPreventsRecovery) {
  RunOnSTAThread(base::BindOnce([] {
    RecoveryTestWindow wnd;
    int fallbacks = 0;
    wnd.set_creation_failed_callback(
        base::BindOnce([](int* count) { ++*count; }, &fallbacks));
    wnd.Init(nullptr, gfx::Rect(0, 0, 100, 100));
    wnd.OnWebMessageReceived(L"ui_ready");
    wnd.OnWebMessageReceived(L"cancel_installation");
    wnd.ScheduleFallback(E_FAIL);
    ::SendMessage(wnd.hwnd(), WM_TIMER, wnd.kInitTimerId, 0);
    ::SendMessage(wnd.hwnd(), WM_TIMER, wnd.kUpdateTimerId, 0);
    wnd.CompleteScript(L"null");
    EXPECT_EQ(fallbacks, 0);
    wnd.Close();
    EXPECT_FALSE(wnd.destroy_handled);
  }));
}

TEST_F(WebView2ProgressWndTest, RetainsApplicationNameBeforePageIsReady) {
  RunOnSTAThread(base::BindOnce([] {
    WebView2ProgressWnd wnd;
    // Progress received before ui_ready must survive in the page payload.
    wnd.OnDownloading("app-id", u"First application", std::nullopt, 42);
    const base::DictValue downloading = wnd.GetUIUpdateForTesting();
    ASSERT_TRUE(downloading.FindString("appName"));
    EXPECT_EQ(*downloading.FindString("appName"), "First application");
    EXPECT_EQ(downloading.FindInt("progress"), 42);
    EXPECT_EQ(downloading.FindBool("isMarquee"), false);

    wnd.OnInstalling("app-id", u"Second application", std::nullopt, 60);
    const base::DictValue installing = wnd.GetUIUpdateForTesting();
    ASSERT_TRUE(installing.FindString("appName"));
    EXPECT_EQ(*installing.FindString("appName"), "Second application");
    EXPECT_EQ(installing.FindInt("progress"), 60);

    wnd.OnWaitingToInstall("app-id", u"Waiting application");
    const base::DictValue waiting = wnd.GetUIUpdateForTesting();
    ASSERT_TRUE(waiting.FindString("appName"));
    EXPECT_EQ(*waiting.FindString("appName"), "Waiting application");
    EXPECT_EQ(waiting.FindBool("isMarquee"), true);
    EXPECT_EQ(waiting.FindBool("complete"), false);
  }));
}

TEST_F(WebView2ProgressWndTest, PreservesDetailedCompletionMessages) {
  RunOnSTAThread(base::BindOnce([] {
    WebView2ProgressWnd wnd;
    ObserverCompletionInfo info = {
        .completion_text = u"Generic failure",
        .apps_info = {{.completion_message = u"Specific application failure"}}};
    // A top-level error overrides an app's default SUCCESS code. Otherwise,
    // an app error overrides top-level SUCCESS, matching the native window.
    for (bool top_level_error : {true, false}) {
      info.completion_code = top_level_error
                                 ? CompletionCodes::COMPLETION_CODE_ERROR
                                 : CompletionCodes::COMPLETION_CODE_SUCCESS;
      info.apps_info.back().completion_code =
          top_level_error ? CompletionCodes::COMPLETION_CODE_SUCCESS
                          : CompletionCodes::COMPLETION_CODE_ERROR;
      wnd.OnComplete(info);
      const base::DictValue update = wnd.GetUIUpdateForTesting();
      ASSERT_TRUE(update.FindString("statusText"));
      EXPECT_EQ(*update.FindString("statusText"),
                "Specific application failure");
      EXPECT_EQ(update.FindBool("complete"), true);
    }
  }));
}

TEST_F(WebView2ProgressWndTest, CompletionMessagesNormalizeLineEndings) {
  RunOnSTAThread(base::BindOnce([] {
    WebView2ProgressWnd wnd;
    ObserverCompletionInfo info = {
        .completion_text = u"First line\r\n\r\nMore detail\nLast line"};
    for (CompletionCodes code : {CompletionCodes::COMPLETION_CODE_SUCCESS,
                                 CompletionCodes::COMPLETION_CODE_ERROR}) {
      info.completion_code = code;
      wnd.OnComplete(info);
      const base::DictValue update = wnd.GetUIUpdateForTesting();
      ASSERT_TRUE(update.FindString("statusText"));
      EXPECT_EQ(*update.FindString("statusText"),
                "First line\n\nMore detail\nLast line");
    }
    info.apps_info.push_back(
        {.completion_message = u"Specific failure\r\nMore detail"});
    wnd.OnComplete(info);
    const base::DictValue update = wnd.GetUIUpdateForTesting();
    ASSERT_TRUE(update.FindString("statusText"));
    EXPECT_EQ(*update.FindString("statusText"),
              "Specific failure\nMore detail");
  }));
}

TEST_F(WebView2ProgressWndTest, DownloadCompletionMatchesNativeUI) {
  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitchASCII(kInstallSwitch,
                                                          "lang=en-US");
  RunOnSTAThread(base::BindOnce([] {
    WebView2ProgressWnd wnd;
    wnd.OnDownloading("app-id", u"App", base::Seconds(1), 99);
    const base::DictValue downloading = wnd.GetUIUpdateForTesting();
    ASSERT_TRUE(downloading.FindString("statusText"));
    EXPECT_EQ(
        *downloading.FindString("statusText"),
        base::WideToUTF8(GetLocalizedString(IDS_DOWNLOADING_BASE, L"en-US")));
    wnd.OnDownloading("app-id", u"App", base::Milliseconds(500), 100);
    const base::DictValue finished = wnd.GetUIUpdateForTesting();
    ASSERT_TRUE(finished.FindString("statusText"));
    EXPECT_EQ(*finished.FindString("statusText"),
              base::WideToUTF8(GetLocalizedString(
                  IDS_DOWNLOADING_COMPLETED_BASE, L"en-US")));
    EXPECT_EQ(finished.FindBool("complete"), false);
    wnd.OnDownloading("app-id", u"App", std::nullopt, 0);
    const base::DictValue indeterminate = wnd.GetUIUpdateForTesting();
    ASSERT_TRUE(indeterminate.FindString("statusText"));
    EXPECT_EQ(
        *indeterminate.FindString("statusText"),
        base::WideToUTF8(GetLocalizedString(IDS_DOWNLOADING_BASE, L"en-US")));
    EXPECT_EQ(indeterminate.FindBool("isMarquee"), true);
  }));
}

TEST_F(WebView2ProgressWndTest, PayloadUsesCurrentBundleName) {
  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitchASCII(kInstallSwitch,
                                                          "lang=en-US");
  RunOnSTAThread(base::BindOnce([] {
    WebView2ProgressWnd wnd;
    for (const std::u16string bundle :
         {u"", u"First bundle", u"Second bundle"}) {
      wnd.set_bundle_name(bundle);
      const base::DictValue update = wnd.GetUIUpdateForTesting();
      ASSERT_TRUE(update.FindString("title"));
      EXPECT_EQ(*update.FindString("title"),
                base::WideToUTF8(GetInstallerDisplayName(bundle, L"en-US")));
    }
  }));
}

TEST_F(WebView2ProgressWndTest, LocalizedPayloadRoundTripsJson) {
  const std::u16string bundle =
      u"Bundle \"quotes\" \\ path\n<script>text</script>";
  for (const std::string language :
       {"en-US", "hi", "ar", "fa", "iw", "he", "ur"}) {
    base::test::ScopedCommandLine command_line;
    command_line.GetProcessCommandLine()->AppendSwitchASCII(kInstallSwitch,
                                                            "lang=" + language);
    RunOnSTAThread(base::BindOnce(
        [](const std::string& language, const std::u16string& bundle) {
          SCOPED_TRACE(language);
          // Construct after setting the tag to exercise production language
          // selection.
          WebView2ProgressWnd wnd;
          wnd.set_bundle_name(bundle);
          wnd.OnDownloading("app-id", bundle, std::nullopt, 42);
          const base::DictValue update = wnd.GetUIUpdateForTesting();
          const std::wstring lang = base::UTF8ToWide(language);
          const LanguageInfo language_info = GetLanguageInfo(lang);
          ASSERT_TRUE(update.FindString("statusText"));
          EXPECT_EQ(
              *update.FindString("statusText"),
              base::WideToUTF8(GetLocalizedString(IDS_DOWNLOADING_BASE, lang)));
          ASSERT_TRUE(update.FindString("title"));
          EXPECT_EQ(*update.FindString("title"),
                    base::WideToUTF8(GetInstallerDisplayName(bundle, lang)));
          ASSERT_TRUE(update.FindString("language"));
          EXPECT_EQ(*update.FindString("language"), language_info.language);
          ASSERT_TRUE(update.FindString("textDirection"));
          EXPECT_EQ(*update.FindString("textDirection"),
                    language_info.is_rtl ? "rtl" : "ltr");
          ASSERT_TRUE(update.FindString("cancelText"));
          EXPECT_EQ(*update.FindString("cancelText"),
                    base::WideToUTF8(GetLocalizedString(
                        IDS_CANCEL_INSTALLATION_BASE, lang)));
          ASSERT_TRUE(update.FindString("cancelingText"));
          EXPECT_EQ(
              *update.FindString("cancelingText"),
              base::WideToUTF8(GetLocalizedString(IDS_CANCELING_BASE, lang)));
          ASSERT_TRUE(update.FindString("appName"));
          EXPECT_EQ(*update.FindString("appName"), base::UTF16ToUTF8(bundle));
          const std::optional<std::string> json = base::WriteJson(update);
          ASSERT_TRUE(json);
          const std::optional<base::Value> decoded =
              base::JSONReader::Read(*json, base::JSON_PARSE_RFC);
          ASSERT_TRUE(decoded);
          EXPECT_EQ(decoded->GetDict(), update);
        },
        language, bundle));
  }
}

TEST_F(WebView2ProgressWndTest, EmbeddedProgressPageIsAvailable) {
  const base::expected<std::wstring, HRESULT> page =
      WebView2ProgressWnd::LoadProgressPage();
  ASSERT_TRUE(page.has_value()) << std::hex << page.error();
  EXPECT_FALSE(page->empty());
}

// The window can't be used if the WebView2 runtime is not installed. This is
// detected before any window is created, allowing native UI fallback.
TEST_F(WebView2ProgressWndTest, InitializeFailsWithoutRuntime) {
  base::ScopedTempDir empty_runtime_folder;
  ASSERT_TRUE(empty_runtime_folder.CreateUniqueTempDir());
  base::ScopedEnvironmentVariableOverride scoped_runtime_folder(
      "WEBVIEW2_BROWSER_EXECUTABLE_FOLDER",
      empty_runtime_folder.GetPath().AsUTF8Unsafe());
  RunOnSTAThread(base::BindOnce([] {
    WebView2ProgressWnd wnd;
    const HRESULT hr = wnd.Initialize();
    EXPECT_TRUE(FAILED(hr)) << std::hex << hr;
    EXPECT_NE(hr, CO_E_NOTINITIALIZED);
  }));
}

// An elevated process refuses the WebView2 UI before touching the runtime; a
// non-elevated process uses the per-user profile regardless of the updater
// scope on the command line. This test runs both non-elevated (updater_tests)
// and elevated (updater_tests_system).
TEST_F(WebView2ProgressWndTest, InitializeDependsOnProcessPrivilege) {
  // Path and command-line overrides must be set up on the main thread.
  base::ScopedTempDir root;
  ASSERT_TRUE(root.CreateUniqueTempDir());
  const base::FilePath local_app_data = root.GetPath().Append(L"Local");
  const base::FilePath program_files = root.GetPath().Append(L"ProgramFiles");
  base::ScopedPathOverride local_override(base::DIR_LOCAL_APP_DATA,
                                          local_app_data);
  base::ScopedPathOverride system_override(base::DIR_PROGRAM_FILESX86,
                                           program_files);
  // The runtime is deliberately absent. A non-elevated process therefore
  // fails after the profile directory is resolved, with an HRESULT other
  // than E_ACCESSDENIED, and no WebView2 process is created.
  base::ScopedEnvironmentVariableOverride runtime_override(
      "WEBVIEW2_BROWSER_EXECUTABLE_FOLDER", root.GetPath().AsUTF8Unsafe());

  const bool is_admin = ::IsUserAnAdmin();
  // Make GetUpdaterScope() the opposite of the process privilege level, so
  // that the profile selection is shown not to depend on it.
  base::test::ScopedCommandLine command_line;
  if (!is_admin) {
    command_line.GetProcessCommandLine()->AppendSwitch(kSystemSwitch);
  }
  const base::FilePath expected_user_data_dir =
      local_app_data.AppendUTF8(COMPANY_SHORTNAME_STRING)
          .AppendUTF8(PRODUCT_FULLNAME_STRING)
          .Append(L"UserData");
  RunOnSTAThread(base::BindOnce(
      [](const base::FilePath& expected_user_data_dir,
         const base::FilePath& program_files, bool is_admin) {
        WebView2ProgressWnd wnd;
        const HRESULT hr = wnd.Initialize();
        EXPECT_TRUE(FAILED(hr)) << std::hex << hr;
        if (is_admin) {
          // Rejected before the runtime check, which would fail differently.
          EXPECT_EQ(hr, E_ACCESSDENIED);
          EXPECT_TRUE(wnd.user_data_dir_for_testing().empty());
        } else {
          EXPECT_NE(hr, E_ACCESSDENIED);
          EXPECT_EQ(wnd.user_data_dir_for_testing(), expected_user_data_dir);
          EXPECT_FALSE(program_files.IsParent(wnd.user_data_dir_for_testing()));
        }
      },
      expected_user_data_dir, program_files, is_admin));
}

}  // namespace updater::ui
