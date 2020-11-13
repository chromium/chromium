// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/test/test_browser_dialog.h"

#include "chrome/browser/ui/views/crostini/crostini_dialogue_browser_test_util.h"
#include "chrome/browser/ui/views/crostini/crostini_package_install_failure_view.h"
#include "content/public/test/browser_test.h"

namespace crostini {
namespace {

class CrostiniPackageInstallFailureViewTest : public DialogBrowserTest {
 public:
  CrostiniPackageInstallFailureViewTest() {}

  void ShowUi(const std::string& name) override {
    CrostiniPackageInstallFailureView::Show("Generic Error Message");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CrostiniPackageInstallFailureViewTest);
};

IN_PROC_BROWSER_TEST_F(CrostiniPackageInstallFailureViewTest,
                       InvokeUi_default) {
  ShowAndVerifyUi();
}

}  // namespace
}  // namespace crostini
