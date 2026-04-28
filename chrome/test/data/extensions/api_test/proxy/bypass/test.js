// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// proxy api test
// browser_tests.exe --gtest_filter=ProxySettingsApiTest.ProxyBypass

function expect(expected, message) {
  return chrome.test.callbackPass(function(value) {
    chrome.test.assertEq(expected, value, message);
  });
}

const HTTP_PROXY = {
  host: '1.1.1.1',
};
const HTTP_PROXY_EXPECTED = {
  scheme: 'http',
  host: '1.1.1.1',
  port: 80,
};

const RULES = {
  proxyForHttp: HTTP_PROXY,
  bypassList: ['localhost', '::1', 'foo.bar', '<local>'],
};
const RULES_EXPECTED = {
  proxyForHttp: HTTP_PROXY_EXPECTED,
  bypassList: ['localhost', '::1', 'foo.bar', '<local>'],
};

const CONFIG = {
  rules: RULES,
  mode: 'fixed_servers',
};
const CONFIG_EXPECTED = {
  rules: RULES_EXPECTED,
  mode: 'fixed_servers',
};

chrome.test.runTests([
  // Verify that execution has started to make sure flaky timeouts are not
  // caused by us.
  function verifyTestsHaveStarted() {
    chrome.test.succeed();
  },
  function setIndividualProxies() {
    chrome.proxy.settings.set({value: CONFIG}, chrome.test.callbackPass());
  },
  function verifyRegular() {
    chrome.proxy.settings.get(
        {incognito: false},
        expect(
            {
              value: CONFIG_EXPECTED,
              levelOfControl: 'controlled_by_this_extension',
            },
            'invalid proxy settings'));
  },
  function verifyIncognito() {
    chrome.proxy.settings.get(
        {incognito: true},
        expect(
            {
              value: CONFIG_EXPECTED,
              incognitoSpecific: false,
              levelOfControl: 'controlled_by_this_extension',
            },
            'invalid proxy settings'));
  },
]);
