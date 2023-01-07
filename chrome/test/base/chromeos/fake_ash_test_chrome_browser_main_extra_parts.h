// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_CHROMEOS_FAKE_ASH_TEST_CHROME_BROWSER_MAIN_EXTRA_PARTS_H_
#define CHROME_TEST_BASE_CHROMEOS_FAKE_ASH_TEST_CHROME_BROWSER_MAIN_EXTRA_PARTS_H_

#include <memory>

#include "base/auto_reset.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"

namespace crosapi {
class TestControllerAsh;
}  // namespace crosapi

namespace test {

class FakeAshTestChromeBrowserMainExtraParts
    : public ChromeBrowserMainExtraParts {
 public:
  FakeAshTestChromeBrowserMainExtraParts();
  FakeAshTestChromeBrowserMainExtraParts(
      const FakeAshTestChromeBrowserMainExtraParts&) = delete;
  FakeAshTestChromeBrowserMainExtraParts& operator=(
      const FakeAshTestChromeBrowserMainExtraParts&) = delete;
  ~FakeAshTestChromeBrowserMainExtraParts() override;

  void PreProfileInit() override;
  void PreBrowserStart() override;
  void PostBrowserStart() override;
  void PostMainMessageLoopRun() override;

 private:
  // Signin errors create a notification. That can interfere with tests.
  std::unique_ptr<base::AutoReset<bool>> ignore_signin_errors_;
  // Multi-device notifications are created on first login.
  std::unique_ptr<base::AutoReset<bool>> ignore_multi_device_notifications_;

  std::unique_ptr<crosapi::TestControllerAsh> test_controller_ash_;
};

}  // namespace test

#endif  // CHROME_TEST_BASE_CHROMEOS_FAKE_ASH_TEST_CHROME_BROWSER_MAIN_EXTRA_PARTS_H_
