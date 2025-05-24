// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Returns a common set of `browser` namespace test cases.
 */
export function getBrowserNamespaceTestCases() {
  return [
    // Tests that `chrome` has extension APIs bound to it.
    async function testChromeBindings() {
      chrome.test.assertTrue(
          !!chrome.runtime,
          'extension API `runtime` inaccessible on `chrome` object');
      chrome.test.succeed();
    },

    // Tests that `browser` has extension APIs bound to it.
    async function testBrowserBindings() {
      chrome.test.assertTrue(
          !!browser.runtime,
          'extension API `runtime` inaccessible on `browser` object');
      chrome.test.assertTrue(
          !browser.loadTimes,
          '`browser` object has access to non-extension APIs (`loadTimes)`');
      chrome.test.assertTrue(
          !browser.app,
          '`browser` object has access to a disallowed APs (`app)`');
      chrome.test.succeed();
    },

    // Tests that the extension API bindings on `chrome` and `browser` point to
    // the same API objects so the two namespace came be used interchangeably.
    async function testChromeAndBrowserBindingsUseSameAPIObjects() {
      chrome.test.assertTrue(
          chrome.runtime === browser.runtime,
          '`chrome` and `browser` objects are not pointing to the same ' +
              'extension API objects');

      function myTestListener() {}
      chrome.runtime.onMessage.addListener(myTestListener);
      chrome.test.assertTrue(
          browser.runtime.onMessage.hasListener(myTestListener),
          '`browser` and `chrome` objects do not share listeners');
      browser.runtime.onMessage.removeListener(myTestListener);
      chrome.test.assertFalse(
          chrome.runtime.onMessage.hasListener(myTestListener),
          '`browser` and `chrome` objects do not share listeners');
      chrome.test.succeed();
    }
  ];
}
