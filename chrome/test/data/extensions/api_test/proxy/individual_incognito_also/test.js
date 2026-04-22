// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// proxy api test
// browser_tests.exe
//     --gtest_filter=ProxySettingsApiTest.ProxyFixedIndividualIncognitoAlso

chrome.test.runTests([
  // Verify that execution has started to make sure flaky timeouts are not
  // caused by us.
  function verifyTestsHaveStarted() {
    chrome.test.succeed();
  },
  function setIndividualProxiesRegular() {
    const httpProxy = {
      host: '1.1.1.1',
    };
    const httpsProxy = {
      scheme: 'socks5',
      host: '2.2.2.2',
    };
    const ftpProxy = {
      host: '3.3.3.3',
      port: 9000,
    };
    const fallbackProxy = {
      scheme: 'socks4',
      host: '4.4.4.4',
      port: 9090,
    };

    const rules = {
      proxyForHttp: httpProxy,
      proxyForHttps: httpsProxy,
      proxyForFtp: ftpProxy,
      fallbackProxy: fallbackProxy,
    };

    const config = {rules: rules, mode: 'fixed_servers'};
    chrome.proxy.settings.set(
        {value: config, scope: 'regular'}, chrome.test.callbackPass());
  },
  function setIndividualProxiesIncognito() {
    const httpProxy = {
      host: '5.5.5.5',
    };
    const httpsProxy = {
      scheme: 'socks5',
      host: '6.6.6.6',
    };
    const ftpProxy = {
      host: '7.7.7.7',
      port: 9000,
    };
    const fallbackProxy = {
      scheme: 'socks4',
      host: '8.8.8.8',
      port: 9090,
    };

    const rules = {
      proxyForHttp: httpProxy,
      proxyForHttps: httpsProxy,
      proxyForFtp: ftpProxy,
      fallbackProxy: fallbackProxy,
    };

    const config = {rules: rules, mode: 'fixed_servers'};
    chrome.proxy.settings.set(
        {value: config, scope: 'incognito_persistent'},
        chrome.test.callbackPass());
  },
]);
