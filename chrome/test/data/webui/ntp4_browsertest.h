// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_DATA_WEBUI_NTP4_BROWSERTEST_H_
#define CHROME_TEST_DATA_WEBUI_NTP4_BROWSERTEST_H_

#include "chrome/test/base/web_ui_browser_test.h"

class NTP4LoggedInWebUITest : public WebUIBrowserTest {
 public:
  NTP4LoggedInWebUITest();

  NTP4LoggedInWebUITest(const NTP4LoggedInWebUITest&) = delete;
  NTP4LoggedInWebUITest& operator=(const NTP4LoggedInWebUITest&) = delete;

  ~NTP4LoggedInWebUITest() override;

 protected:
  // Sets the user name in the profile as if the user had logged in.
  void SetLoginName(const std::string& name);
};

#endif  // CHROME_TEST_DATA_WEBUI_NTP4_BROWSERTEST_H_
