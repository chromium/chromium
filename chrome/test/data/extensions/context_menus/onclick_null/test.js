// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.contextMenus.create({
  id: 'id1',
  title: 'Menu item 1',
  onclick: function() {
    chrome.test.sendMessage('onclick1-unexpected');
  }
}, onCreatedFirstMenu);

function onCreatedFirstMenu() {
  chrome.contextMenus.update('id1', {
    onclick: null
  }, function() {
    chrome.test.sendMessage('update1', function() {
      // Now create another context menu item, to test whether adding and
      // updating a context menu with a new onclick handler works.
      // Upon completing that test, we will also know whether menu 1's initial
      // onclick attribute has been triggered unexpectedly.
      createSecondMenu();
    });
  });
}

function createSecondMenu() {
  chrome.contextMenus.create({
    id: 'id2',
    title: 'Menu item 2',
    onclick: function() {
      chrome.test.sendMessage('onclick2-unexpected');
    }
  }, function() {
    chrome.contextMenus.update('id2', {
      onclick: function() {
        chrome.test.sendMessage('onclick2');
      }
    }, function() {
      chrome.test.sendMessage('update2');
    });
  });
}
