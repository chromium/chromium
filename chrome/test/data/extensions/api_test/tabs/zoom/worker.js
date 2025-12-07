// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests that calling a zoom function on a page that can't be zoomed
// (because it is native UI) does not crash the browser.
async function testNoCrashOnNativeUiTab() {
  // Create a tab that uses native UI, not WebUI, on Android.
  const tab = await chrome.tabs.create({url: 'chrome://downloads'});
  chrome.test.assertNoLastError();

  // Trying to check zoom level should fail but not crash.
  try {
    await chrome.tabs.getZoom();
  } catch (e) {
    chrome.test.assertEq(
        e.message, 'Cannot get zoom or zoom settings on this tab.');
    chrome.test.succeed();
    return;
  }
  chrome.test.fail();
}

chrome.test.runTests([testNoCrashOnNativeUiTab]);
