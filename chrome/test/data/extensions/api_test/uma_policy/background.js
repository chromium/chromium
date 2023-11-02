// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var done = false;
var first = false;
var allComplete = false;

chrome.runtime.onMessage.addListener(
  function(request, sender, sendResponse) {
    if (request['testType'] == 'single') {
      done = true;
    } else if (request['testType'] == 'double') {
      if (first)
        done = true;
      else
        first = true;
    }
    if (done) {
      chrome.tabs.query({url: 'http://www.blocker.com/'}, function(tabs) {
          chrome.tabs.update(tabs[0]['id'], {url: 'http://www.done.com'});
        });
      allComplete = true;
    }
  });

chrome.tabs.onUpdated.addListener(
  function(tabid, changeinfo, tab) {
    if (done && !allComplete) {
      if (changeinfo.url == 'http://www.blocker.com/') {
        chrome.tabs.update(tabid, {url: 'http://www.done.com'});
      }
    }
  });
