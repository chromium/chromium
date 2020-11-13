// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_DIALOGUE_BROWSER_TEST_UTIL_H_
#define CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_DIALOGUE_BROWSER_TEST_UTIL_H_

#include <memory>

#include "chrome/browser/chromeos/crostini/crostini_browser_test_util.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"

namespace content {
class WebContents;
}

// Common base for Crostini dialog browser tests. Allows tests to set network
// connection type.
class CrostiniDialogBrowserTest
    : public SupportsTestDialog<CrostiniBrowserTestBase> {
 public:
  explicit CrostiniDialogBrowserTest(bool register_termina)
      : SupportsTestDialog<CrostiniBrowserTestBase>(register_termina) {}

  void WaitForLoadFinished(content::WebContents* contents);

 private:
  DISALLOW_COPY_AND_ASSIGN(CrostiniDialogBrowserTest);
};

#endif  // CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_DIALOGUE_BROWSER_TEST_UTIL_H_
