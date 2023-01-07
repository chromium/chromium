// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// proxy api test
// browser_tests.exe --gtest_filter=ProxySettingsApiTest.ProxyPacData

function expect(expected, message) {
  return chrome.test.callbackPass(function(value) {
    chrome.test.assertEq(expected, value, message);
  });
}

var pacScriptObject = {
  data: "function FindProxyForURL(url, host) {\n" +
        "  if (host == 'foobar.com')\n" +
        "    return 'PROXY blackhole:80';\n" +
        "  return 'DIRECT';\n" +
        "}",
  mandatory: false
};
var config = {
  mode: "pac_script",
  pacScript: pacScriptObject
};

chrome.test.runTests([
  // Verify that execution has started to make sure flaky timeouts are not
  // caused by us.
  function verifyTestsHaveStarted() {
    chrome.test.succeed();
  },
  function setAutoSettings() {
    chrome.proxy.settings.set(
        {'value': config},
        chrome.test.callbackPass());
  },
  function verifySettings() {
    chrome.proxy.settings.get(
        {'incognito': false},
        expect({ 'value': config,
                 'levelOfControl': "controlled_by_this_extension" },
               "invalid proxy settings"));
  }
]);
