// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests that calling a zoom function on a native UI page does not crash
// the browser and returns a valid zoom level.
async function testNoCrashOnNativeUiTab() {
  // Create a tab that uses native UI, not WebUI, on Android.
  const tab = await chrome.tabs.create({url: 'chrome://downloads'});
  chrome.test.assertNoLastError();

  // Checking zoom level should succeed without crashing.
  const zoom = await chrome.tabs.getZoom(tab.id);
  chrome.test.assertTrue(typeof zoom === 'number' && zoom > 0);
  chrome.test.succeed();
}

function waitForTabComplete(tabId) {
  return new Promise(resolve => {
    chrome.tabs.onUpdated.addListener(function listener(id, changeInfo) {
      if (id === tabId && changeInfo.status === 'complete') {
        chrome.tabs.onUpdated.removeListener(listener);
        resolve();
      }
    });
  });
}

async function testZoomOnWebTab() {
  const tab = await chrome.tabs.create({url: 'about:blank'});
  chrome.test.assertNoLastError();
  await waitForTabComplete(tab.id);

  // Test getZoom on a normal web tab returns a valid zoom factor.
  const initialZoom = await chrome.tabs.getZoom(tab.id);
  chrome.test.assertTrue(typeof initialZoom === 'number' && initialZoom > 0);
  const settings = await chrome.tabs.getZoomSettings(tab.id);
  chrome.test.assertTrue(
      typeof settings.defaultZoomFactor === 'number' &&
      settings.defaultZoomFactor > 0);

  // Test setZoom and verify getZoom returns the updated value.
  // When a zoom factor is set, the reported zoom factor is scaled by the
  // tab's initial/default zoom factor (1.0 on standard desktop, or 1.09 on
  // Android due to OS-level font scale and display density adjustments).
  await chrome.tabs.setZoom(tab.id, 1.5);
  const newZoom = await chrome.tabs.getZoom(tab.id);
  chrome.test.assertTrue(
      Math.abs(newZoom - (1.5 * initialZoom)) < 0.001,
      `Expected newZoom (${newZoom}) to equal 1.5 * initialZoom (${
          1.5 * initialZoom})`);

  // Test getZoomSettings.
  chrome.test.assertEq('automatic', settings.mode);
  chrome.test.assertEq('per-origin', settings.scope);

  // Test setZoomSettings.
  await chrome.tabs.setZoomSettings(tab.id, {mode: 'manual', scope: 'per-tab'});
  const updatedSettings = await chrome.tabs.getZoomSettings(tab.id);
  chrome.test.assertEq('manual', updatedSettings.mode);
  chrome.test.assertEq('per-tab', updatedSettings.scope);

  chrome.test.succeed();
}

chrome.test.runTests([testNoCrashOnNativeUiTab, testZoomOnWebTab]);
