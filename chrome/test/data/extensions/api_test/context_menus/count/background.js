// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See MenuManager::kMaxItemsPerExtension.
const maxMenuItems = 1000;

chrome.test.runTests([
  async function createTooManyMenuItems() {
    // No menus should be initially created, since this extension only
    // runs once, but remove all just in case.
    await chrome.contextMenus.removeAll();

    // Create the maximum allowed number of menus.
    for (let i = 0; i < maxMenuItems; ++i) {
      await new Promise((resolve) => {
        chrome.contextMenus.create(
            {title: `Test item ${i}`,
             id: `item ${i}`},
            () => {
              chrome.test.assertNoLastError();
              resolve();
            })});
    }

    // Try to create one more over the limit.
    chrome.contextMenus.create(
        {title: `Test item ${maxMenuItems}`,
         id: `item ${maxMenuItems}`},
        () => {
          chrome.test.assertLastError(
              `An extension can create a maximum of ${maxMenuItems} menu ` +
              'items.');
          chrome.test.succeed();
        });
  },
]);
