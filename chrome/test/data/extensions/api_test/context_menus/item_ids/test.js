// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const inServiceWorker = 'ServiceWorkerGlobalScope' in self;

chrome.test.runTests([
  function stringID() {
    var id1 = chrome.contextMenus.create(
        {"id": "id1", "title": "title1"}, function() {
          chrome.test.assertNoLastError();
          chrome.test.assertEq("id1", id1);
          chrome.contextMenus.remove("id1", chrome.test.callbackPass());
    });
  },

  function parentStringID() {
    chrome.contextMenus.create({"id": "id1", "title": "title1"}, function() {
      chrome.test.assertNoLastError();
      chrome.contextMenus.create(
          {"id": "id2", "title": "title2", "parentId": "id1"}, function() {
        chrome.test.assertNoLastError();
        chrome.contextMenus.create(
            {"id": "id3", "title": "title3"}, function() {
          chrome.test.assertNoLastError();
          chrome.contextMenus.update("id3", {"parentId": "id1"},
                                     chrome.test.callbackPass());
        });
      });
    });
  },

  function idCollision() {
    chrome.contextMenus.create({"id": "mine", "title": "first"}, function() {
      chrome.contextMenus.create({"id": "mine", "title": "second"},
      chrome.test.callbackFail("Cannot create item with duplicate id mine"));
    });
  },

  function idNonCollision() {
    // This test is not valid for SW-based extensions and will be removed
    // soon when integer IDs are no longer supported for context menus.
    // See crbug.com/1044784.
    if (inServiceWorker) {
      chrome.test.succeed();
      return;
    }
    var intId = chrome.contextMenus.create({"title": "int17"}, function() {
      chrome.test.assertNoLastError();
      var stringId = String(intId);
      chrome.contextMenus.create(
          {"id": stringId, "title": "string17"}, function() {
        chrome.test.assertNoLastError();
        chrome.contextMenus.remove(intId, function() {
          chrome.test.assertNoLastError();
          chrome.contextMenus.remove(stringId, chrome.test.callbackPass());
        });
      });
    });
  }
]);
