// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_TEST_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_TEST_H_

#include "base/macros.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"

namespace web_app {

class WebAppTest : public ChromeRenderViewHostTestHarness {
 public:
  WebAppTest();
  ~WebAppTest() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebAppTest);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_TEST_H_
