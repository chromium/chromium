// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.contextMenus.create({title: "popup", id: 'popup2'}, function(menu) {
  chrome.test.assertNoLastError();
  chrome.test.notifyPass();
});
