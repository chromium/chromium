// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This works-around that content scripts can't import because they aren't
// considered modules.
(async () => {
  const src = chrome.runtime.getURL(
      '/_test_resources/api_test/browser_object/' +
      'browser_object_common_tests.js');
  const testUtil = await import(src);
  chrome.test.runTests(testUtil.getBrowserNamespaceTestCases());
})();
