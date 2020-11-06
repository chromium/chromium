// Copyright 2020 The Chromium Authors. All rights reserved.
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

  const MESSAGE_INVALID_SCHEME = new RegExp(
      'The scheme of the url provided must be \'https\' or ' +
      '\'chrome-extension\'.');

  const MESSAGE_MISSING_PERCENT =
      /The url provided \(.+\) does not contain '%s'/;

  const MESSAGE_INVALID_URL_CREATED = new RegExp(
      'The custom handler URL created by removing \'%s\' and prepending ' +
      '\'.+\' is invalid');

  chrome.test.runTests([
    function invalidURL() {
      chrome.test.assertThrows(
          navigator.registerProtocolHandler, navigator,
          ['mailto', 'invalidurl://%s', TITLE], MESSAGE_INVALID_SCHEME);
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
