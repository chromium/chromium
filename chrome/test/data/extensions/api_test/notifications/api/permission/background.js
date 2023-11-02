// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var currentPermissionLevel = "";

function onGetPermissionLevel(permissionLevel) {
  currentPermissionLevel = permissionLevel;
}

function getPermissionLevel() {
  chrome.notifications.getPermissionLevel(onGetPermissionLevel);
}

function onPermissionLevelChangedListener(permissionLevel) {
  if (permissionLevel != "denied" && permissionLevel != "granted") {
    chrome.test.notifyFail(
        "Unexpected permission level " + permissionLevel + " received");
    return;
  }

  if (permissionLevel == currentPermissionLevel) {
    chrome.test.notifyFail(
        "Same permission level " + permissionLevel + " received");
    return;
  }

  currentPermissionLevel = permissionLevel;
  chrome.test.notifyPass();
}

getPermissionLevel();
chrome.notifications.onPermissionLevelChanged.addListener(
    onPermissionLevelChangedListener);
