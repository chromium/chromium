// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/ui/webview2_progress_wnd.h"

#include <windows.h>

#include <optional>
#include <string>
#include <utility>

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
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/updater/app/app_install_progress.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/win/ui/l10n_util.h"
#include "chrome/updater/win/ui/resources/updater_installer_strings.h"
#include "chrome/updater/win/ui/ui_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater::ui {
namespace {

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
    ObserverCompletionInfo info;
    info.completion_text = u"Generic failure";
    info.apps_info.emplace_back();
    info.apps_info.back().completion_message = u"Specific application failure";
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
    ObserverCompletionInfo info;
    info.completion_text = u"First line\r\n\r\nMore detail\nLast line";
    for (CompletionCodes code : {CompletionCodes::COMPLETION_CODE_SUCCESS,
                                 CompletionCodes::COMPLETION_CODE_ERROR}) {
      info.completion_code = code;
      wnd.OnComplete(info);
      const base::DictValue update = wnd.GetUIUpdateForTesting();
      ASSERT_TRUE(update.FindString("statusText"));
      EXPECT_EQ(*update.FindString("statusText"),
                "First line\n\nMore detail\nLast line");
    }
    info.apps_info.emplace_back();
    info.apps_info.back().completion_message =
        u"Specific failure\r\nMore detail";
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

}  // namespace
}  // namespace updater::ui
