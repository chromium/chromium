// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/ui/webview2ui.h"

#include <windows.h>

#include <wrl/client.h>
#include <wrl/implements.h>

#include <string>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/scoped_environment_variable_override.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater::ui {
namespace {

// Creates and destroys a plain top-level window to use as the WebView2 parent.
class ScopedParentWindow {
 public:
  ScopedParentWindow()
      : hwnd_(::CreateWindowExW(0,
                                L"STATIC",
                                L"",
                                WS_POPUP,
                                0,
                                0,
                                100,
                                100,
                                nullptr,
                                nullptr,
                                nullptr,
                                nullptr)) {}
  ScopedParentWindow(const ScopedParentWindow&) = delete;
  ScopedParentWindow& operator=(const ScopedParentWindow&) = delete;
  ~ScopedParentWindow() {
    if (hwnd_) {
      ::DestroyWindow(hwnd_);
    }
  }

  HWND hwnd() const { return hwnd_; }

 private:
  const HWND hwnd_;
};

class ProcessFailureArgs final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          ICoreWebView2ProcessFailedEventArgs> {
 public:
  explicit ProcessFailureArgs(COREWEBVIEW2_PROCESS_FAILED_KIND kind)
      : kind_(kind) {}
  HRESULT STDMETHODCALLTYPE
  get_ProcessFailedKind(COREWEBVIEW2_PROCESS_FAILED_KIND* kind) override {
    *kind = kind_;
    return S_OK;
  }

 private:
  COREWEBVIEW2_PROCESS_FAILED_KIND kind_;
};

class NavigationResultArgs final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          ICoreWebView2NavigationCompletedEventArgs> {
 public:
  explicit NavigationResultArgs(BOOL success) : success_(success) {}
  HRESULT STDMETHODCALLTYPE get_IsSuccess(BOOL* success) override {
    *success = success_;
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE
  get_WebErrorStatus(COREWEBVIEW2_WEB_ERROR_STATUS* status) override {
    *status = COREWEBVIEW2_WEB_ERROR_STATUS_CONNECTION_ABORTED;
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE get_NavigationId(UINT64* id) override {
    *id = 1;
    return S_OK;
  }

 private:
  BOOL success_;
};

}  // namespace

class WebView2UITest : public ::testing::Test {
 protected:
  // Runs `WebView2UI::Create` on a COM STA thread, as the updater UI thread is,
  // and returns its synchronous result. The creation callback must not run:
  // it fails the test if it does. The result is delivered back on this
  // sequence, which `TestFuture` requires.
  HRESULT CreateOnSTAThread(HWND parent, const base::FilePath& user_data_dir) {
    base::test::TestFuture<HRESULT> result;
    base::ThreadPool::CreateCOMSTATaskRunner({base::MayBlock()})
        ->PostTaskAndReplyWithResult(
            FROM_HERE,
            base::BindOnce(
                [](HWND parent, const base::FilePath& user_data_dir) {
                  WebView2UI webview;
                  return webview.Create(
                      parent, RECT{0, 0, 100, 100}, user_data_dir,
                      base::BindOnce([](HRESULT) {
                        ADD_FAILURE() << "The creation callback must not run "
                                         "for a synchronous failure.";
                      }));
                },
                parent, user_data_dir),
            result.GetCallback());
    return result.Get();
  }

  base::test::TaskEnvironment task_environment_;
};

TEST_F(WebView2UITest, ProcessFailuresAreClassified) {
  WebView2UI webview;
  int failures = 0;
  webview.SetFailureHandler(base::BindRepeating(
      [](int* count, HRESULT hr) {
        EXPECT_TRUE(FAILED(hr));
        ++*count;
      },
      &failures));
  for (auto kind :
       {COREWEBVIEW2_PROCESS_FAILED_KIND_GPU_PROCESS_EXITED,
        COREWEBVIEW2_PROCESS_FAILED_KIND_UTILITY_PROCESS_EXITED,
        COREWEBVIEW2_PROCESS_FAILED_KIND_SANDBOX_HELPER_PROCESS_EXITED,
        COREWEBVIEW2_PROCESS_FAILED_KIND_PPAPI_PLUGIN_PROCESS_EXITED,
        COREWEBVIEW2_PROCESS_FAILED_KIND_PPAPI_BROKER_PROCESS_EXITED,
        COREWEBVIEW2_PROCESS_FAILED_KIND_FRAME_RENDER_PROCESS_EXITED,
        COREWEBVIEW2_PROCESS_FAILED_KIND_UNKNOWN_PROCESS_EXITED}) {
    auto args = Microsoft::WRL::Make<ProcessFailureArgs>(kind);
    EXPECT_EQ(webview.OnProcessFailed(nullptr, args.Get()), S_OK);
  }
  EXPECT_EQ(failures, 0);
  for (auto kind :
       {COREWEBVIEW2_PROCESS_FAILED_KIND_BROWSER_PROCESS_EXITED,
        COREWEBVIEW2_PROCESS_FAILED_KIND_RENDER_PROCESS_EXITED,
        COREWEBVIEW2_PROCESS_FAILED_KIND_RENDER_PROCESS_UNRESPONSIVE}) {
    auto args = Microsoft::WRL::Make<ProcessFailureArgs>(kind);
    const int previous = failures;
    EXPECT_EQ(webview.OnProcessFailed(nullptr, args.Get()), S_OK);
    EXPECT_EQ(failures, previous + 1);
  }
}

TEST_F(WebView2UITest, NavigationFailuresAreReported) {
  WebView2UI webview;
  int failures = 0;
  webview.SetFailureHandler(base::BindRepeating(
      [](int* count, HRESULT hr) {
        EXPECT_TRUE(FAILED(hr));
        ++*count;
      },
      &failures));
  auto success = Microsoft::WRL::Make<NavigationResultArgs>(TRUE);
  EXPECT_EQ(webview.OnNavigationCompleted(nullptr, success.Get()), S_OK);
  EXPECT_EQ(failures, 0);
  auto failure = Microsoft::WRL::Make<NavigationResultArgs>(FALSE);
  EXPECT_EQ(webview.OnNavigationCompleted(nullptr, failure.Get()), S_OK);
  EXPECT_EQ(failures, 1);
  EXPECT_EQ(webview.OnNavigationCompleted(nullptr, nullptr), S_OK);
  EXPECT_EQ(failures, 2);
}

TEST_F(WebView2UITest, ScriptErrorsCompleteCallback) {
  WebView2UI webview;
  base::test::TestFuture<HRESULT> failure;
  webview.SetFailureHandler(failure.GetRepeatingCallback());
  base::test::TestFuture<std::wstring> result;
  webview.OnScriptExecuted(result.GetCallback<const std::wstring&>(), E_FAIL,
                           L"true");
  EXPECT_EQ(failure.Get(), E_FAIL);
  EXPECT_TRUE(result.Get().empty());
  base::test::TestFuture<std::wstring> success;
  webview.OnScriptExecuted(success.GetCallback<const std::wstring&>(), S_OK,
                           L"true");
  EXPECT_EQ(success.Get(), L"true");
}

TEST_F(WebView2UITest, ExecuteWithoutWebViewNotifiesAndCompletes) {
  WebView2UI webview;
  base::test::TestFuture<HRESULT> failure;
  webview.SetFailureHandler(failure.GetRepeatingCallback());
  base::test::TestFuture<std::wstring> result;
  EXPECT_EQ(
      webview.ExecuteScript(L"true", result.GetCallback<const std::wstring&>()),
      E_POINTER);
  EXPECT_EQ(failure.Get(), E_POINTER);
  EXPECT_TRUE(result.Get().empty());
}

TEST_F(WebView2UITest, CreateWithoutParentFails) {
  EXPECT_EQ(CreateOnSTAThread(nullptr, base::FilePath(L"C:\\UserData")),
            E_INVALIDARG);
}

TEST_F(WebView2UITest, CreateFailsSynchronouslyWithoutRuntime) {
  // Pointing the WebView2 loader at an empty folder makes it behave as if no
  // WebView2 runtime is installed.
  base::ScopedTempDir empty_runtime_folder;
  ASSERT_TRUE(empty_runtime_folder.CreateUniqueTempDir());
  base::ScopedEnvironmentVariableOverride scoped_runtime_folder(
      "WEBVIEW2_BROWSER_EXECUTABLE_FOLDER",
      empty_runtime_folder.GetPath().AsUTF8Unsafe());

  base::ScopedTempDir user_data_dir;
  ASSERT_TRUE(user_data_dir.CreateUniqueTempDir());

  // The parent window is created on this thread. WebView2 only needs a valid
  // handle to start the environment creation.
  ScopedParentWindow parent;
  ASSERT_TRUE(parent.hwnd());

  // `CreateOnSTAThread` returns after the `WebView2UI` has been destroyed on
  // the STA thread, so nothing outlives the parent window.
  EXPECT_TRUE(
      FAILED(CreateOnSTAThread(parent.hwnd(), user_data_dir.GetPath())));
}

}  // namespace updater::ui
