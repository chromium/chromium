// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// proxy api test
// browser_tests.exe
//     --gtest_filter=ProxySettingsApiTest.ProxyFixedIndividualRemove

const HTTP_PROXY = {
  host: '1.1.1.1',
};
const HTTPS_PROXY = {
  scheme: 'socks5',
  host: '2.2.2.2',
};
const FTP_PROXY = {
  host: '3.3.3.3',
  port: 9000,
};
const FALLBACK_PROXY = {
  scheme: 'socks4',
  host: '4.4.4.4',
  port: 9090,
};

const RULES = {
  proxyForHttp: HTTP_PROXY,
  proxyForHttps: HTTPS_PROXY,
  proxyForFtp: FTP_PROXY,
  fallbackProxy: FALLBACK_PROXY,
};

const CONFIG = {
  rules: RULES,
  mode: 'fixed_servers',
};

chrome.test.runTests([
  // Verify that execution has started to make sure flaky timeouts are not
  // caused by us.
  function verifyTestsHaveStarted() {
    chrome.test.succeed();
  },
  function setIndividualProxies() {
    chrome.proxy.settings.set(
        {value: CONFIG, scope: 'regular'}, chrome.test.callbackPass());
  },
  function clearProxies() {
    chrome.proxy.settings.clear({scope: 'regular'}, chrome.test.callbackPass());
  },
]);
