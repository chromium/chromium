// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var inIncognitoContext = chrome.extension.inIncognitoContext;
var incognitoStr = inIncognitoContext ? 'incognito' : 'regular';

chrome.runtime.onInstalled.addListener(function(details) {
  chrome.contextMenus.onClicked.addListener(function(info, tab) {
    chrome.test.sendMessage('onclick fired ' + incognitoStr);
  });

  chrome.contextMenus.create(
      {title: 'item ' + incognitoStr, id: 'id_' + incognitoStr},
      function() {
        chrome.test.assertNoLastError();
        chrome.test.sendMessage('created item ' + incognitoStr);
      })
});
