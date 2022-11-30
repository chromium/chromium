// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.bookmarks.onCreated.addListener(
    function() {
      window.called = true;
      chrome.test.sendMessage('ready', function() {});
    });
