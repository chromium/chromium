// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function isRegexSupported(regexOptions) {
  return new Promise(resolve => {
    chrome.declarativeNetRequest.isRegexSupported(regexOptions, resolve);
  });
}

chrome.test.runTests([
  async function testSupportedRegex() {
    let result = await isRegexSupported({regex: '[0-9]+'});
    chrome.test.assertEq(result, {isSupported: true});
    chrome.test.succeed();
  },

  async function testSupportedRegexWithOptions() {
    let result = await isRegexSupported(
        {regex: '[0-9]+', isCaseSensitive: false, requireCapturing: true});
    chrome.test.assertEq(result, {isSupported: true});
    chrome.test.succeed();
  },

  async function testInvalidRegex() {
    let result = await isRegexSupported({regex: '[a-9]+'});
    chrome.test.assertEq(result, {isSupported: false, reason: 'syntaxError'});
    chrome.test.succeed();
  },

  async function testInvalidRegexWithOptions() {
    let result = await isRegexSupported(
        {regex: '[a-9]+', isCaseSensitive: false, requireCapturing: true});
    chrome.test.assertEq(result, {isSupported: false, reason: 'syntaxError'});
    chrome.test.succeed();
  },

  async function testMemoryError() {
    let result = await isRegexSupported({regex: '[0-9]+'.repeat(1000)});
    chrome.test.assertEq(
        result, {isSupported: false, reason: 'memoryLimitExceeded'});
    chrome.test.succeed();
  },

  async function testMemoryErrorWithOptions() {
    let regex = '(a)'.repeat(50);
    let result = await isRegexSupported({regex});
    chrome.test.assertEq(result, {isSupported: true});
    result = await isRegexSupported({regex, requireCapturing: true});
    chrome.test.assertEq(
        result, {isSupported: false, reason: 'memoryLimitExceeded'});
    chrome.test.succeed();
  }
]);
