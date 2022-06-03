// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var pass = chrome.test.callbackPass;

chrome.tabs.getSelected(null, function(tab) {
  chrome.browserAction.setPopup({tabId: tab.id, popup: 'newPopup.html'})
  chrome.browserAction.setTitle({tabId: tab.id, title: 'newTitle'});
  chrome.browserAction.setBadgeBackgroundColor({
    tabId: tab.id,
    color: [0, 0, 0, 0]
  });
  chrome.browserAction.setBadgeText({tabId: tab.id, text: 'newText'});
  chrome.browserAction.setBadgeText({text: 'defaultText'});

  chrome.test.runTests([
    function getBadgeText() {
      chrome.browserAction.getBadgeText({tabId: tab.id}, pass(function(result) {
                                          chrome.test.assertEq(
                                              'newText', result);
                                        }));
    },

    // Sanity check that specifying an empty string for setBadgeText will set
    // the badge text to be empty instead of reset it to the default badge text.
    function emptyTabBadgeText() {
      chrome.browserAction.setBadgeText(
          {tabId: tab.id, text: ''}, pass(function() {
            chrome.browserAction.getBadgeText(
                {tabId: tab.id}, pass(function(result) {
                  chrome.test.assertEq('', result);
                }));
          }));
    },

    // The badge text shown should be the default badge text (if set) after the
    // tab specific badge text is removed via setBadgeText({tabId: tab.id}).
    function clearTabBadgeText() {
      chrome.browserAction.setBadgeText(
          {tabId: tab.id}, pass(function() {
            chrome.browserAction.getBadgeText(
                {tabId: tab.id}, pass(function(result) {
                  chrome.test.assertEq('defaultText', result);
                }));
          }));
    },

    // The default badge text should be removed after setBadgeText({}) is
    // called.
    function clearGlobalBadgeText() {
      chrome.browserAction.setBadgeText({}, pass(function() {
                                          chrome.browserAction.getBadgeText(
                                              {}, pass(function(result) {
                                                chrome.test.assertEq(
                                                    '', result);
                                              }));
                                        }));
    },

    function getBadgeBackgroundColor() {
      chrome.browserAction.getBadgeBackgroundColor({tabId: tab.id},
                                                   pass(function(result) {
        chrome.test.assertEq([0, 0, 0, 0], result);
      }));
    },

    function getPopup() {
      chrome.browserAction.getPopup({tabId: tab.id}, pass(function(result) {
        chrome.test.assertTrue(
            /chrome-extension\:\/\/[a-p]{32}\/newPopup\.html/.test(result));
      }));
    },

    function getTitle() {
      chrome.browserAction.getTitle({tabId: tab.id}, pass(function(result) {
                                      chrome.test.assertEq('newTitle', result);
                                    }));
    }
  ]);
});
