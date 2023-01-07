// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_CHROMEOS_TEST_CHROME_BASE_H_
#define CHROME_TEST_BASE_CHROMEOS_TEST_CHROME_BASE_H_

#include "base/memory/weak_ptr.h"
#include "content/public/app/content_main.h"
#include "content/public/common/main_function_params.h"

namespace content {
class BrowserMainParts;
}
class ChromeBrowserMainParts;

namespace test {

class TestChromeBase {
 public:
  explicit TestChromeBase(content::ContentMainParams params);
  TestChromeBase(const TestChromeBase&) = delete;
  TestChromeBase& operator=(const TestChromeBase&) = delete;
  ~TestChromeBase();

  // Start the browser.
  int Start();

  // Captures |browser_main_parts_| so main parts can be added in tests.
  void CreatedBrowserMainPartsImpl(
      content::BrowserMainParts* browser_main_parts);

  // Create fake ash browser main extra parts.
  void CreateFakeAshTestChromeBrowserMainExtraParts();

 private:
  content::ContentMainParams params_;
  ChromeBrowserMainParts* browser_main_parts_ = nullptr;
  base::WeakPtrFactory<TestChromeBase> weak_ptr_factory_{this};
};

}  // namespace test

#endif  // CHROME_TEST_BASE_CHROMEOS_TEST_CHROME_BASE_H_
