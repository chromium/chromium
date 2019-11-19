// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var inIncognitoContext = chrome.extension.inIncognitoContext;
var incognitoStr = inIncognitoContext ? 'incognito' : 'regular';

chrome.contextMenus.create({title: 'item ' + incognitoStr,
                            id: 'id_' + incognitoStr}, function() {
  chrome.test.assertNoLastError();
  chrome.contextMenus.onClicked.addListener(function(info, tab) {
    chrome.test.sendMessage('onclick fired ' + incognitoStr);
  });
  chrome.test.sendMessage('created item ' + incognitoStr);
});
