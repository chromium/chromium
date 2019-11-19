// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function stringID() {
    var id1 = chrome.contextMenus.create(
        {"id": "id1", "title": "title1"}, function() {
          chrome.test.assertNoLastError();
          chrome.test.assertEq("id1", id1);
          chrome.contextMenus.remove("id1", chrome.test.callbackPass());
    });
  },

  function generatedID() {
    chrome.contextMenus.create(
        {"title": "title2"},
        chrome.test.callbackFail("Extensions using event pages or Service " +
                                 "Workers must pass an id parameter to " +
                                 "chrome.contextMenus.create"));
  },

  function noOnClick() {
    chrome.contextMenus.create(
        {"id": "id3", "title": "title3", "onclick": function() {}},
        chrome.test.callbackFail(
            "Extensions using event pages or Service Workers cannot pass an " +
            "onclick parameter to chrome.contextMenus.create. Instead, use " +
            "the chrome.contextMenus.onClicked event."));
  }
]);
