// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const inIncognitoContext = chrome.extension.inIncognitoContext;
const message = inIncognitoContext ? 'waiting_incognito' : 'waiting';

function reportSuccess() {
  chrome.test.sendMessage(message);
}

chrome.browserAction.onClicked.addListener(function() {
  reportSuccess();
});

chrome.bookmarks.onCreated.addListener(function() {
  reportSuccess();
});
