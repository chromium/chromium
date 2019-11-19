// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.management.onInstalled.addListener(function(extensionInfo) {
  if (!extensionInfo.isApp) {
    console.log("Can't launch " + extensionInfo.name + " (" +
                extensionInfo.id + "): Not an app.");
    return;
  }
  console.log("Launch " + extensionInfo.name + " (" +
              extensionInfo.id + ")");

  chrome.management.launchApp(extensionInfo.id);
});

chrome.test.sendMessage("launcher loaded");
