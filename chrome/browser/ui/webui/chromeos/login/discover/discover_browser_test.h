// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_DISCOVER_DISCOVER_BROWSER_TEST_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_DISCOVER_DISCOVER_BROWSER_TEST_H_

#include <string>

#include "base/macros.h"
#include "chrome/test/base/in_process_browser_test.h"

class Browser;
class Profile;

namespace content {
class WebContents;
}

namespace chromeos {
class DiscoverUI;

namespace test {
class JSChecker;
}  // namespace test

// Base class for Discover tests.
// CHECK()'s that there is only one instance of DiscoverUI.
class DiscoverBrowserTest : public InProcessBrowserTest {
 public:
  DiscoverBrowserTest() = default;
  ~DiscoverBrowserTest() override = default;

  // Loads Discover App for `profile` and blocks until JS reports "initialized".
  void LoadAndInitializeDiscoverApp(Profile* profile);

  // Returns WebContents of a loaded Discover App.
  content::WebContents* GetWebContents() const;

  // Returns Discover UI
  DiscoverUI* GetDiscoverUI() const;

  // Returns JS checker attached to the Discover App WebContents.
  test::JSChecker DiscoverJS() const;

  // Clicks on `card_selector` Discover Card.
  void ClickOnCard(const std::string& card_selector) const;

 private:
  // Triggers Discover App display and blocks until it is loaded.
  void LoadDiscoverApp(Profile* profile);

  // Block until Discover App is initialized.
  void InitializeDiscoverApp() const;

  Browser* discover_browser_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(DiscoverBrowserTest);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_DISCOVER_DISCOVER_BROWSER_TEST_H_
