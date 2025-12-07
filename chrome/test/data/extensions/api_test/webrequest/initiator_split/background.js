// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.initiators = [];

function onBeforeRequest(details) {
  if (details.initiator && details.url.includes('title1.html')) {
    self.initiators.push(details.initiator);
  }
}

chrome.webRequest.onBeforeRequest.addListener(
    onBeforeRequest, {types: ['sub_frame'], urls: ['<all_urls>']});

var readyMessage =
    chrome.extension.inIncognitoContext ? 'incognito ready' : 'ready';
chrome.test.sendMessage(readyMessage);
