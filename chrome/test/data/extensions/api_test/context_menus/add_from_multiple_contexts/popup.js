// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.contextMenus.create({title: "menu", id: 'popup'}, function(menu) {
  chrome.test.assertNoLastError();
  chrome.test.notifyPass();
});
