// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_BROWSER_INFO_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_BROWSER_INFO_H_

#include "base/values.h"
#include "chrome/test/chromedriver/chrome/devtools_endpoint.h"
#include "chrome/test/chromedriver/chrome/status.h"

// Content Shell and WebView have an empty product version and a fake user
// agent. There's no way to detect the actual version, so unless specified we
// assume it is tip of tree.
static const int kToTBuildNo = 9999;

// Similarly, if the Blink Revision isn't given then assume it is tip of tree.
static const int kToTBlinkRevision = 999999;

struct BrowserInfo {
  BrowserInfo();
  BrowserInfo(const BrowserInfo&);
  BrowserInfo(BrowserInfo&&);
  BrowserInfo& operator=(const BrowserInfo&);
  BrowserInfo& operator=(BrowserInfo&&);
  ~BrowserInfo();

  std::string android_package;
  std::string browser_name;
  std::string browser_version;
  std::string web_socket_url;
  DevToolsEndpoint debugger_endpoint;
  int major_version = 0;
  int build_no = kToTBuildNo;
  int blink_revision = kToTBlinkRevision;
  bool is_android = false;
  bool is_headless_shell = false;

  Status FillFromBrowserVersionResponse(const base::Value::Dict& response);

  Status ParseBrowserInfo(const std::string& data);

  static Status ParseBrowserInfo(const std::string& data,
                                 BrowserInfo* browser_info);

  static Status ParseBrowserString(bool has_android_package,
                                   const std::string& browser_string,
                                   BrowserInfo* browser_info);

  static Status ParseBrowserVersionString(const std::string& browser_version,
                                          int* major_version,
                                          int* build_no);

  static Status ParseBlinkVersionString(const std::string& blink_version,
                                        int* blink_revision);

  static bool IsGitHash(const std::string& revision);
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_BROWSER_INFO_H_
