// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function createMenuItems() {
  // Create two parent items with two children each.
  let parents = [];
  parents.push(chrome.contextMenus.create({
    title: 'Test parent item 1',
    id: 'parent1'
  }));
  parents.push(chrome.contextMenus.create({
    title: 'Test parent item 2',
    id: 'parent2'
  }));

  let currentId = 1;
  parents.forEach((parent) => {
    let idString = 'child' + currentId;
    ++currentId;
    chrome.contextMenus.create({
      title: 'Child 1',
      parentId: parent,
      id: idString
    });

    idString = 'child' + currentId;
    ++currentId;
    chrome.contextMenus.create({
      title: 'Child 2',
      parentId: parent,
      id: idString
    });
  });
}

chrome.runtime.onInstalled.addListener(() => {
  // Create the menu items and signal success.
  createMenuItems();
  chrome.test.assertNoLastError();
  chrome.test.notifyPass();
});
