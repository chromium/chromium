// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// proxy api test
// browser_tests.exe --gtest_filter=ProxySettingsApiTest.ProxyPacDataUrl

chrome.test.runTests([
  // Verify that execution has started to make sure flaky timeouts are not
  // caused by us.
  function verifyTestsHaveStarted() {
    chrome.test.succeed();
  },
  function setAutoSettings() {
    var kExpectedPacScript =
        "function FindProxyForURL(url, host) {\n" +
        "  if (host == 'foobar.com')\n" +
        "    return 'PROXY blackhole:80';\n" +
        "  return 'DIRECT';\n" +
        "}";
    var pacScriptObject = {
      url: "data:;base64,ZnVuY3Rpb24gRmluZFByb3h5Rm9yVVJMKHVybCwgaG9zdCkgewo" +
           "gIGlmIChob3N0ID09ICdmb29iYXIuY29tJykKICAgIHJldHVybiAnUFJPWFkgYmx" +
           "hY2tob2xlOjgwJzsKICByZXR1cm4gJ0RJUkVDVCc7Cn0="
    };
    var config = {
      mode: "pac_script",
      pacScript: pacScriptObject
    };
    chrome.proxy.settings.set(
        {'value': config},
        function() {
          chrome.proxy.settings.get(
              {},
              function(config) {
                chrome.test.assertEq(kExpectedPacScript,
                                     config.value.pacScript.data);
                chrome.test.succeed();
              });
        })
  }
]);
