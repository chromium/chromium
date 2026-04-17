// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const inIncognitoContext = chrome.extension.inIncognitoContext;
const incognitoStr = inIncognitoContext ? 'incognito' : 'regular';

chrome.contextMenus.onClicked.addListener(function(info, tab) {
  chrome.test.sendMessage('onclick fired ' + incognitoStr);
});

chrome.contextMenus.create(
    {title: 'item ' + incognitoStr, id: 'id_' + incognitoStr}, function() {
      chrome.test.assertNoLastError();
      chrome.test.sendMessage('created item ' + incognitoStr);
    });
