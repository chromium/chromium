// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// browser_tests --gtest_filter=ProtocolHandlerApiTest.Registration

function registerProtocolHandlerWithUserGesture(scheme, url, title) {
  return new Promise((resolve, reject) => {
    window.addEventListener('message', function(event) {
      chrome.test.assertEq('observing_change', event.data);
      try {
        window.addEventListener('message', function(event) {
          chrome.test.assertEq('change_observed', event.data);
          resolve();
        }, {once: true});
        navigator.registerProtocolHandler(scheme, url, title);
      } catch (error) {
        reject(error);
      }
    }, {once: true});
    chrome.test.sendMessage('request_register_protocol');
  });
}

function verifyRegistration(scheme) {
  const url = `${scheme}:path`;
  const a = document.body.appendChild(document.createElement('a'));
  a.href = url;
  a.rel = 'opener';
  a.target = '_blank';
  return new Promise((resolve, reject) => {
    window.addEventListener('message', function(event) {
      chrome.test.assertEq(url, event.data.protocol);
      event.source.close();
      resolve();
    }, {once: true})
    a.click();
  });
}

async function testRegisterProtocolHandler(scheme, url, title) {
  await registerProtocolHandlerWithUserGesture(scheme, url, title);
  await verifyRegistration(scheme);
}

chrome.test.getConfig(function(config) {
  const CROSS_ORIGIN_LOCALHOST_URL =
      crossOriginLocalhostURLFromPort(config.testServer.port) +
      'handler.html?protocol=%s';

  const MESSAGE_INVALID_URI_SYNTAX =
      /The scheme name 'ext\+@' is not allowed by URI syntax \(RFC3986\)./;

  const MESSAGE_NOT_SAFELISTED = new RegExp(
      'The scheme \'unknownscheme\' doesn\'t belong to the scheme allowlist. ' +
      'Please prefix non-allowlisted schemes with the string \'web\\+\'');

  const MESSAGE_EXT_PLUS_SCHEME = new RegExp(
      'The scheme name \'ext\\+\' is not allowed. Schemes starting with ' +
      '\'ext\\+\' must be followed by one or more ASCII letters.');

  const MESSAGE_INVALID_SCHEME =
      /The scheme of the url provided must be HTTP\(S\)./;

  const MESSAGE_MISSING_PERCENT =
      /The url provided \(.+\) does not contain '%s'/;

  const MESSAGE_INVALID_URL_CREATED = new RegExp(
      'The custom handler URL created by removing \'%s\' and prepending ' +
      '\'.+\' is invalid');

  chrome.test.runTests([
    function invalidScheme() {
      chrome.test.assertThrows(
          navigator.registerProtocolHandler, navigator,
          ['ext+@', SAME_ORIGIN_CHROME_EXTENSION_URL, TITLE],
          MESSAGE_INVALID_URI_SYNTAX);
      chrome.test.assertThrows(
          navigator.registerProtocolHandler, navigator,
          ['unknownscheme', SAME_ORIGIN_CHROME_EXTENSION_URL, TITLE],
          MESSAGE_NOT_SAFELISTED);
      chrome.test.assertThrows(
          navigator.registerProtocolHandler, navigator,
          ['ext+', SAME_ORIGIN_CHROME_EXTENSION_URL, TITLE],
          MESSAGE_EXT_PLUS_SCHEME);
      chrome.test.succeed();
    },

    function invalidURL() {
      chrome.test.assertThrows(
          navigator.registerProtocolHandler, navigator,
          ['mailto', 'invalidurl://%s', TITLE], MESSAGE_INVALID_SCHEME);
      chrome.test.assertThrows(
          navigator.registerProtocolHandler, navigator,
          ['mailto', `blob:${SAME_ORIGIN_CHROME_EXTENSION_URL}`, TITLE],
          MESSAGE_INVALID_SCHEME);
      chrome.test.assertThrows(
          navigator.registerProtocolHandler, navigator,
          ['mailto', 'data:text/html,Hello?url=%s', TITLE],
          MESSAGE_INVALID_SCHEME);
      chrome.test.assertThrows(
          navigator.registerProtocolHandler, navigator,
          ['mailto', `filesystem:${SAME_ORIGIN_CHROME_EXTENSION_URL}`, TITLE],
          MESSAGE_INVALID_SCHEME);
      chrome.test.assertThrows(
          navigator.registerProtocolHandler, navigator,
          ['mailto', chrome.runtime.getURL('xhr.txt'), TITLE],
          MESSAGE_MISSING_PERCENT);
      chrome.test.assertThrows(
          navigator.registerProtocolHandler, navigator,
          ['mailto', 'https://%s', TITLE], MESSAGE_INVALID_URL_CREATED);
      chrome.test.succeed();
    },

    async function chromeExtensionURL() {
      chrome.test.assertTrue(
          SAME_ORIGIN_CHROME_EXTENSION_URL.startsWith('chrome-extension://'));
      await testRegisterProtocolHandler(
          'mailto', SAME_ORIGIN_CHROME_EXTENSION_URL, TITLE);
      chrome.test.succeed();
    },

    async function extPlusFooScheme() {
      await testRegisterProtocolHandler(
          'ext+foo', SAME_ORIGIN_CHROME_EXTENSION_URL, TITLE);
      chrome.test.succeed();
    },

    async function crossOriginURL() {
      chrome.test.assertFalse(
          CROSS_ORIGIN_LOCALHOST_URL.startsWith(location.origin));
      await testRegisterProtocolHandler(
          'irc', CROSS_ORIGIN_LOCALHOST_URL, TITLE);
      chrome.test.succeed();
    },

    async function finalizeTests() {
      await new Promise((resolve, reject) => {
        window.addEventListener('message', function(event) {
          chrome.test.assertEq('complete', event.data);
          chrome.test.succeed();
        }, {once: true});
        chrome.test.sendMessage('request_complete');
      });
    }
  ]);
});
