// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var inIncognitoContext = chrome.extension.inIncognitoContext;
var message = inIncognitoContext ? "waiting_incognito" : "waiting";

function reportSuccess() {
  chrome.test.sendMessage(message);
}

chrome.browserAction.onClicked.addListener(function() {
  reportSuccess();
});

chrome.bookmarks.onCreated.addListener(function() {
  reportSuccess();
});
