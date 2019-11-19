// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_MACHINE_LEARNING_MACHINE_LEARNING_INTERNALS_BROWSERTEST_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_MACHINE_LEARNING_MACHINE_LEARNING_INTERNALS_BROWSERTEST_H_

#include "chrome/test/base/mojo_web_ui_browser_test.h"

class MachineLearningInternalsBrowserTest : public MojoWebUIBrowserTest {
 public:
  MachineLearningInternalsBrowserTest();
  ~MachineLearningInternalsBrowserTest() override;

 protected:
  // Initialize a fake service connection which simply returns given
  // fake_output. Browser test for ml internal page uses this function to set
  // fake service connection for testing.
  void SetupFakeConnectionAndOutput(double fake_output);

  DISALLOW_COPY_AND_ASSIGN(MachineLearningInternalsBrowserTest);
};

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_MACHINE_LEARNING_MACHINE_LEARNING_INTERNALS_BROWSERTEST_H_

