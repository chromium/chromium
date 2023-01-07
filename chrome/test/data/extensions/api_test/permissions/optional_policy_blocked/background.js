// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var assertTrue = chrome.test.assertTrue;
var fail = chrome.test.callbackFail;
var pass = chrome.test.callbackPass;

var BLOCKED_BY_ENTERPRISE_ERROR =
    "Permissions are blocked by enterprise policy.";

chrome.test.getConfig(function(config) {

  chrome.test.runTests([
    function allowedPermission() {
      chrome.permissions.request(
          {permissions:['bookmarks']},
          pass(function(granted) { assertTrue(granted); }));
    },

    function allowedPermission() {
      chrome.permissions.request(
          {permissions:['management']},
          fail(BLOCKED_BY_ENTERPRISE_ERROR));
    }
  ]);
});
