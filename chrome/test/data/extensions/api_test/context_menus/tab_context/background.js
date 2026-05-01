// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  async function createTabContextMenu() {
    const eventPromise = chrome.test.listenOnce(chrome.contextMenus.onClicked);

    await chrome.contextMenus.create({
      title: 'Test Tab Item',
      contexts: ['tab'],
      id: 'test_item',
    });

    // Send a message to the C++ to indicate the item has been created.
    // The C++ side will verify its presence in the menu and click on it.
    chrome.test.sendMessage('created');

    // Which should trigger our listener.
    const args = await eventPromise;
    chrome.test.assertEq(2, args.length);

    const info = args[0];
    chrome.test.assertEq('test_item', info.menuItemId);
    chrome.test.assertEq(false, info.editable);
    chrome.test.assertEq('about:blank', info.pageUrl);

    const tab = args[1];
    chrome.test.assertTrue(Number.isInteger(tab.id));
    chrome.test.assertTrue(Number.isInteger(tab.windowId));

    chrome.test.succeed();
  },
]);
