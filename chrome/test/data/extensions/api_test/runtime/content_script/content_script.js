// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var assertFalse = chrome.test.assertFalse;
var assertTrue = chrome.test.assertTrue;

function isAvailable(api) {
  if (!(api in chrome.runtime))
    return false;

  try {
    var mightThrowException = chrome.runtime[api];
  } catch (e) {
    return false;
  }

  // Defined and doesn't throw an exception on access. It's available.
  return true;
}

assertTrue(chrome.hasOwnProperty('runtime'), 'chrome.runtime not defined.');

// Would test lastError but it's only defined when there's been an error.
assertTrue(isAvailable('id'), 'id not available.');
assertTrue(isAvailable('getManifest'), 'getManifest not available');
assertTrue(isAvailable('getURL'), 'getManifest not available');
assertFalse(isAvailable('getBackgroundPage'), 'getBackgroundPage available');
assertFalse(isAvailable('onInstalled'), 'onInstalled available');
assertFalse(isAvailable('onSuspend'), 'onSuspend available');

chrome.test.notifyPass();
