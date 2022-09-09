// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

if (typeof pass_exported == 'undefined')
  chrome.test.notifyFail('pass.js was not exported correctly.');

chrome.runtime.onInstalled.addListener(function(details) {
  if (details.reason == 'shared_module_update' &&
      details.previousVersion == '1.0' &&
      // This ID corresponds to the public key in 'shared'.
      details.id == 'gpcckkmippodnppallflahfabmeilgjg') {
    chrome.test.sendMessage('shared_module_updated');
  }
});

chrome.test.sendMessage('ready');

chrome.test.notifyPass();
