// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([async function createMultipleTabContextMenuItems() {
  const onClickedPromise =
      chrome.test.listenOnce(chrome.contextMenus.onClicked);

  await chrome.contextMenus.create({
    id: 'item_1',
    title: 'Submenu Item 1',
    contexts: ['tab'],
  });
  await chrome.contextMenus.create({
    id: 'item_2',
    title: 'Submenu Item 2',
    contexts: ['tab'],
  });

  // Send a message to C++ indicating the items are created.
  chrome.test.sendMessage('created');

  // Wait for the click event to fire and verify the correct item ID.
  const args = await onClickedPromise;
  chrome.test.assertEq(2, args.length);
  const info = args[0];
  chrome.test.assertEq('item_1', info.menuItemId);
  chrome.test.succeed();
}]);
