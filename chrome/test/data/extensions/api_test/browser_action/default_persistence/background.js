// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Set a background color for the badge at OnInstalled().
chrome.runtime.onInstalled.addListener(function() {
  chrome.browserAction.setBadgeBackgroundColor({color: [0, 0, 255, 255]});
  chrome.test.sendMessage('Background Color Set');
});

// Every startup, change the badge text, but not the background color (which
// should persist at the blue we set originally).
chrome.runtime.onStartup.addListener(function() {
  chrome.browserAction.setBadgeText({text: 'Hello'});
  chrome.test.sendMessage('Badge Text Set');
});
