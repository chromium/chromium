// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function testWebNavigationOnCommitted() {
    var getURL = chrome.extension.getURL;
    chrome.tabs.create({url: 'about:blank'}, function(tab) {
      var tabId = tab.id;
      var aVisited = false;
      chrome.webNavigation.onCommitted.addListener(function(details) {
        chrome.test.fail();
      }, {url: [{pathSuffix: 'never-navigated.html'}]});
      chrome.webNavigation.onCommitted.addListener(function(details) {
        chrome.test.log('chrome.webNavigation.onCommitted - a.html');
        chrome.test.assertEq(getURL('a.html'), details.url);
        aVisited = true;
      }, {url: [{pathSuffix: 'a.html'}]});
      chrome.webNavigation.onCommitted.addListener(function(details) {
        chrome.test.log('chrome.webNavigation.onCommitted - b.html');
        chrome.test.assertEq(getURL('b.html'), details.url);
        chrome.test.assertTrue(aVisited);
        chrome.test.succeed();
      }, {url: [{pathSuffix: 'b.html'}]});

      chrome.tabs.update(tabId, {url: getURL('a.html')});
    });
  }
]);
