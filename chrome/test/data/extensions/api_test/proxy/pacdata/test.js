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

const PAC_SCRIPT_OBJECT = {
  data: `function FindProxyForURL(url, host) {
  if (host == 'foobar.com')
    return 'PROXY blackhole:80';
  return 'DIRECT';
}`,
  mandatory: false,
};
const CONFIG = {
  mode: 'pac_script',
  pacScript: PAC_SCRIPT_OBJECT,
};

chrome.test.runTests([
  // Verify that execution has started to make sure flaky timeouts are not
  // caused by us.
  function verifyTestsHaveStarted() {
    chrome.test.succeed();
  },
  function setAutoSettings() {
    chrome.proxy.settings.set({value: CONFIG}, chrome.test.callbackPass());
  },
  function verifySettings() {
    chrome.proxy.settings.get(
        {incognito: false},
        expect(
            {value: CONFIG, levelOfControl: 'controlled_by_this_extension'},
            'invalid proxy settings'));
  },
]);
