// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// proxy api test
// browser_tests.exe --gtest_filter=ProxySettingsApiTest.ProxyFixedSingle

chrome.test.runTests([
  // Verify that execution has started to make sure flaky timeouts are not
  // caused by us.
  function verifyTestsHaveStarted() {
    chrome.test.succeed();
  },
  function setSingleProxy() {
    var oneProxy = {
      host: "127.0.0.1",
      port: 100
    };

    var rules = {
      singleProxy: oneProxy
    };

    var config = { rules: rules, mode: "fixed_servers" };
    chrome.proxy.settings.set(
        {'value': config},
        chrome.test.callbackPass());
  }
]);
