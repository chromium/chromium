// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_CHROMEOS_ASH_BROWSER_TEST_STARTER_H_
#define CHROME_TEST_BASE_CHROMEOS_ASH_BROWSER_TEST_STARTER_H_

#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_feature_list.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/aura/window_observer.h"
#include "url/gurl.h"

class InProcessBrowserTest;

namespace aura {
class Window;
}

namespace test {

class AshBrowserTestStarter : public aura::WindowObserver {
 public:
  AshBrowserTestStarter();
  AshBrowserTestStarter(const AshBrowserTestStarter&) = delete;
  AshBrowserTestStarter& operator=(const AshBrowserTestStarter&) = delete;
  ~AshBrowserTestStarter() override;

  // Returns whether the --lacros-chrome-path is provided.
  // If returns false, we should not do any Lacros related testing
  // because the Lacros instance is not provided.
  bool HasLacrosArgument() const;

  // Prepares ash so it can work with Lacros. You should call this
  // in SetUpInProcessBrowserTestFixture().
  // Returns true if everything works.
  [[nodiscard]] bool PrepareEnvironmentForLacros();

  // Starts Lacros and waits for it's fully started. You should call
  // this no earlier than SetUpOnMainThread().
  void StartLacros(InProcessBrowserTest* test_class_obj);

  net::EmbeddedTestServer* https_server();
  GURL base_url();

  // Returns the initial Lacros window. Will CHECK-fail if the window has
  // already been destroyed.
  aura::Window* initial_lacros_window() {
    CHECK(initial_lacros_window_);
    return initial_lacros_window_;
  }

 private:
  // aura::WindowObserver
  void OnWindowDestroying(aura::Window* window) override;

  // This is XDG_RUNTIME_DIR.
  base::ScopedTempDir scoped_temp_dir_xdg_;

  base::test::ScopedFeatureList scoped_feature_list_;
  net::EmbeddedTestServer https_server_;

  // Used for clean up.
  base::FilePath ash_user_data_dir_for_cleanup_;

  // See initial_lacros_window().
  raw_ptr<aura::Window> initial_lacros_window_ = nullptr;
};

}  // namespace test

#endif  // CHROME_TEST_BASE_CHROMEOS_ASH_BROWSER_TEST_STARTER_H_
