// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/510816360): Remove this histogram test.

// extension api test
// browser_tests.exe \
// --gtest_filter=ExtensionApiTest.ChromeRuntimeUninstallURLHistogramTest

chrome.test.runTests([
  async function setUninstallURL() {
    // Set URL twice, only the second call (secure URL) should count.
    await chrome.runtime.setUninstallURL('http://example.com');
    await chrome.runtime.setUninstallURL('https://example.com');
    chrome.test.succeed();
  },
]);
