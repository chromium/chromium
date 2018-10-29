// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_BROWSER_TEST_UTIL_H_
#define CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_BROWSER_TEST_UTIL_H_

#include <memory>

#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/test/base/browser_process_platform_part_test_api_chromeos.h"
#include "net/base/network_change_notifier.h"

class CrostiniBrowserTestChromeBrowserMainExtraParts;

// Common base for Crostini dialog broswer tests. Allows tests to set network
// connection type.
class CrostiniDialogBrowserTest : public DialogBrowserTest {
 public:
  explicit CrostiniDialogBrowserTest(bool register_termina);
  ~CrostiniDialogBrowserTest() override;

  // BrowserTestBase:
  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override;
  void SetUp() override;
  void SetUpOnMainThread() override;

  void SetConnectionType(
      net::NetworkChangeNotifier::ConnectionType connection_type);

  void UnregisterTermina();

 protected:
  const bool register_termina_;

  // Owned by content::Browser
  CrostiniBrowserTestChromeBrowserMainExtraParts* extra_parts_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(CrostiniDialogBrowserTest);
};

#endif  // CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_BROWSER_TEST_UTIL_H_
