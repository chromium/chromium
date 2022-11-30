// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_MANAGEMENT_MANAGEMENT_A11Y_BROWSERTEST_H_
#define CHROME_BROWSER_UI_WEBUI_MANAGEMENT_MANAGEMENT_A11Y_BROWSERTEST_H_

#include "base/files/file_path.h"
#include "build/chromeos_buildflags.h"
#include "chrome/test/base/web_ui_browser_test.h"
#include "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"

// C++ test fixture used by management_a11y_test.js.
class ManagementA11yUIBrowserTest : public WebUIBrowserTest {
 public:
  ManagementA11yUIBrowserTest();
  ~ManagementA11yUIBrowserTest() override;

 protected:
  void InstallPowerfulPolicyEnforcedExtension();
  const base::FilePath test_data_dir_;
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  policy::FakeBrowserDMTokenStorage fake_dm_token_storage_;
#endif
};

#endif  // CHROME_BROWSER_UI_WEBUI_MANAGEMENT_MANAGEMENT_A11Y_BROWSERTEST_H_
