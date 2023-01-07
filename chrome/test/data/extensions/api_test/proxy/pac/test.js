// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// proxy api test
// browser_tests.exe --gtest_filter=ProxySettingsApiTest.ProxyPacScript

chrome.test.runTests([
  // Verify that execution has started to make sure flaky timeouts are not
  // caused by us.
  function verifyTestsHaveStarted() {
    chrome.test.succeed();
  },
  function setAutoSettings() {
    var pacScriptObject = {
      url: "http://wpad/windows.pac"
    };
    var config = {
      mode: "pac_script",
      pacScript: pacScriptObject
    };
    chrome.proxy.settings.set(
        {'value': config},
        chrome.test.callbackPass());
  }
]);
