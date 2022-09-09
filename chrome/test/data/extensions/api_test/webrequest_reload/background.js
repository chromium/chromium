// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.webRequest.onBeforeRequest.addListener(
  function(details) {
  },
  {
    urls: [],
    types: []
  },
  []);
if (chrome.extension.inIncognitoContext)
  chrome.test.sendMessage("done_incognito");
else
  chrome.test.sendMessage("done");
