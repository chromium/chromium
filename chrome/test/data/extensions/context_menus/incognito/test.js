// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var inIncognitoContext = chrome.extension.inIncognitoContext;
var incognitoStr = inIncognitoContext ? "incognito" : "regular";

function onclick(info) {
  chrome.test.sendMessage("onclick fired " + incognitoStr);
}

chrome.contextMenus.create({title: "item " + incognitoStr,
                            onclick: onclick}, function() {
  if (!chrome.runtime.lastError) {
    chrome.test.sendMessage("created item " + incognitoStr);
  }
});
