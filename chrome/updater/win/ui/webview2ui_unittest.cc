// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/ui/webview2ui.h"

#include <windows.h>

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

}  // namespace
}  // namespace updater::ui
