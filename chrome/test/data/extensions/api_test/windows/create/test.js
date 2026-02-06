// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

async function isAndroid() {
  const info = await chrome.runtime.getPlatformInfo();
  return info.os === 'android';
}

chrome.test.runTests([
  async function typeNormal() {
    const window = await chrome.windows.create({'type': 'normal'});
    chrome.test.assertEq('normal', window.type);
    await chrome.windows.remove(window.id);  // cleanup
    chrome.test.succeed();
  },
  async function typePopup() {
    const w = await chrome.windows.create({'type': 'popup'});
    chrome.test.assertEq('popup', w.type);
    await chrome.windows.remove(w.id);  // cleanup
    chrome.test.succeed();
  },
  function sizeTooBig() {
    // Setting origin + bad width/height should not crash.
    chrome.windows.create({
      'type': 'normal',
      'left': 0,
      'top': 0,
      'width': 2147483647,
      'height': 2147483647,
    }, (w => {
      chrome.test.assertLastError('Invalid value for bounds. Bounds must be ' +
                                  'at least 50% within visible screen space.');
      chrome.test.succeed();
    }));
  },
  async function createDataHasTabId() {
    // Arrange: Create a new window with one tab and get that tab's ID.
    const window1 = await chrome.windows.create({
      'type': 'normal',
      'url': ['a.html'],
    });
    chrome.test.assertEq(1, window1.tabs.length);
    const tabId = window1.tabs[0].id;

    // Act: Create another window with the tab ID we got from the 1st window.
    const window2 = await chrome.windows.create({
      'type': 'normal',
      'tabId': tabId,
    });

    // Assert: The 2nd window should have one active tab with the 1st window's
    // tab ID.
    chrome.test.assertEq(1, window2.tabs.length);
    chrome.test.assertTrue(window2.tabs[0].active);
    chrome.test.assertEq(tabId, window2.tabs[0].id);

    // Cleanup.
    //
    // Note that on Android, a window will show the grid tab switcher when
    // there are no tabs, so we should explicitly remove window1.
    //
    // Window1 will be automatically closed on Windows/Mac/Linux.
    if (await isAndroid()) {
      await chrome.windows.remove(window1.id);
    }
    await chrome.windows.remove(window2.id);

    chrome.test.succeed();
  },
  async function createDataHasUrlAndTabId() {
    // Arrange: Create a new window with one tab and get that tab's ID.
    const window1 = await chrome.windows.create({
      'type': 'normal',
      'url': ['a.html'],
    });
    chrome.test.assertEq(1, window1.tabs.length);
    const tabId = window1.tabs[0].id;

    // Act: Create another window with a URL and the tab ID we got from the 1st
    // window.
    const window2 = await chrome.windows.create({
      'type': 'normal',
      'url': ['b.html'],
      'tabId': tabId,
    });

    // Assert: The 2nd window should have two tabs.
    chrome.test.assertEq(2, window2.tabs.length);

    // Assert: The 1st tab should be active.
    chrome.test.assertTrue(window2.tabs[0].active);
    chrome.test.assertFalse(window2.tabs[1].active);

    // Assert: The 1st tab should be b.html.
    // Note that navigation is asynchronous, so b.html is either "pendingUrl"
    // or "url".
    chrome.test.assertFalse(tabId === window2.tabs[0].id);
    chrome.test.assertEq(
        chrome.runtime.getURL('b.html'),
        window2.tabs[0].pendingUrl ?? window2.tabs[0].url);

    // Assert: The 2nd tab should be the "a.html" tab from the 1st window.
    chrome.test.assertEq(tabId, window2.tabs[1].id);
    chrome.test.assertEq(
        chrome.runtime.getURL('a.html'),
        window2.tabs[1].pendingUrl ?? window2.tabs[1].url);

    // Cleanup.
    //
    // Note that on Android, a window will show the grid tab switcher when
    // there are no tabs, so we should explicitly remove window1.
    //
    // Window1 will be automatically closed on Windows/Mac/Linux.
    if (await isAndroid()) {
      await chrome.windows.remove(window1.id);
    }
    await chrome.windows.remove(window2.id);

    chrome.test.succeed();
  },
]);
