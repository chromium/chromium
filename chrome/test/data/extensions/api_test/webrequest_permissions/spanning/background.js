// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.declarativeWebRequest.onRequest.removeRules();
var rule = {
  conditions: [
    new chrome.declarativeWebRequest.RequestMatcher(
        {url: {pathContains: '.html'}}),
  ],
  actions: [
    new chrome.declarativeWebRequest.RedirectRequest(
        {'redirectUrl': 'data:text/plain,redirected1'})
  ],
}
function notifyDone() {
  chrome.test.sendMessage("done");
}
chrome.declarativeWebRequest.onRequest.addRules([rule], notifyDone);
