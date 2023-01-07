// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.tabs.onCreated.addListener(function onCreated(tab) {
  chrome.tabs.onCreated.removeListener(onCreated);
  chrome.extension.getBackgroundPage().verifyCreatedTab(tab);
});
window.open('foo.html');
window.close();
