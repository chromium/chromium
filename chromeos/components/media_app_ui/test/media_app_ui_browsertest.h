// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_MEDIA_APP_UI_TEST_MEDIA_APP_UI_BROWSERTEST_H_
#define CHROMEOS_COMPONENTS_MEDIA_APP_UI_TEST_MEDIA_APP_UI_BROWSERTEST_H_

#include <memory>

#include "base/macros.h"
#include "chrome/test/base/mojo_web_ui_browser_test.h"

class MediaAppUiBrowserTest : public MojoWebUIBrowserTest {
 public:
  MediaAppUiBrowserTest();
  ~MediaAppUiBrowserTest() override;

  // MojoWebUIBrowserTest:
  void SetUpOnMainThread() override;

 private:
  class TestCodeInjector;
  std::unique_ptr<TestCodeInjector> injector_;

  DISALLOW_COPY_AND_ASSIGN(MediaAppUiBrowserTest);
};

#endif  // CHROMEOS_COMPONENTS_MEDIA_APP_UI_TEST_MEDIA_APP_UI_BROWSERTEST_H_
