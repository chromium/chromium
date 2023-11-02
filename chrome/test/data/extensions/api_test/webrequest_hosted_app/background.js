// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
chrome.webRequest.onBeforeRequest.addListener(function(details) {
    chrome.test.sendMessage(details.type);
}, {
    urls: ['*://*/*webrequest_hosted_app*']
});
