// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_CHROMEOS_TEST_LACROS_CHROME_BROWSER_MAIN_EXTRA_PARTS_H_
#define CHROME_TEST_BASE_CHROMEOS_TEST_LACROS_CHROME_BROWSER_MAIN_EXTRA_PARTS_H_

#include <memory>

#include "chrome/browser/chrome_browser_main_extra_parts.h"

class StandaloneBrowserTestController;

namespace test {

// This class adds test-only features to Lacros. It is used to build the
// test_lacros_chrome binary, which is meant to be run together with certain Ash
// browser tests. See also docs/lacros/test_linux_lacros.md.
class TestLacrosChromeBrowserMainExtraParts
    : public ChromeBrowserMainExtraParts {
 public:
  TestLacrosChromeBrowserMainExtraParts();
  TestLacrosChromeBrowserMainExtraParts(
      const TestLacrosChromeBrowserMainExtraParts&) = delete;
  TestLacrosChromeBrowserMainExtraParts& operator=(
      const TestLacrosChromeBrowserMainExtraParts&) = delete;
  ~TestLacrosChromeBrowserMainExtraParts() override;

  void PostBrowserStart() override;

 private:
  // A test controller that is registered with Ash's TestController
  // service over crosapi to let Ash tests control this Lacros
  // instance.
  std::unique_ptr<StandaloneBrowserTestController>
      standalone_browser_test_controller_;
};

}  // namespace test

#endif  // CHROME_TEST_BASE_CHROMEOS_TEST_LACROS_CHROME_BROWSER_MAIN_EXTRA_PARTS_H_
