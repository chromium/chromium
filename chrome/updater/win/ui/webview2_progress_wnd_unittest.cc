// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/ui/webview2_progress_wnd.h"

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

// Constructs a `WebView2ProgressWnd` and calls `Initialize()` on it.
HRESULT InitializeProgressWnd() {
  WebView2ProgressWnd wnd;
  return wnd.Initialize();
}

class WebView2ProgressWndTest : public ::testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

// The window can't be used if the WebView2 runtime is not installed. This is
// detected by `Initialize()`, before any window is created, so that the caller
// can fall back to a different UI.
TEST_F(WebView2ProgressWndTest, InitializeFailsWithoutRuntime) {
  // Pointing the WebView2 loader at an empty folder makes it behave as if no
  // WebView2 runtime is installed.
  base::ScopedTempDir empty_runtime_folder;
  ASSERT_TRUE(empty_runtime_folder.CreateUniqueTempDir());
  base::ScopedEnvironmentVariableOverride scoped_runtime_folder(
      "WEBVIEW2_BROWSER_EXECUTABLE_FOLDER",
      empty_runtime_folder.GetPath().AsUTF8Unsafe());

  // The updater UI thread is a COM STA. The result is delivered back on this
  // sequence, which `TestFuture` requires.
  base::test::TestFuture<HRESULT> result;
  base::ThreadPool::CreateCOMSTATaskRunner({base::MayBlock()})
      ->PostTaskAndReplyWithResult(FROM_HERE,
                                   base::BindOnce(&InitializeProgressWnd),
                                   result.GetCallback());

  const HRESULT hr = result.Get();
  EXPECT_TRUE(FAILED(hr)) << std::hex << hr;

  // The failure must come from the runtime check, not from the apartment
  // check, which passes on an STA thread.
  EXPECT_NE(hr, CO_E_NOTINITIALIZED);
}

}  // namespace
}  // namespace updater::ui
