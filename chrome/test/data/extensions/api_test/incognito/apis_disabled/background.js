// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var normalWindow, normalTab;

var pass = chrome.test.callbackPass;
var assertEq = chrome.test.assertEq;
var assertTrue = chrome.test.assertTrue;

chrome.test.runTests([
  function getAllWindows() {
    // The test harness should have set us up with 2 windows: 1 incognito
    // and 1 regular. We should only see the regular one.
    chrome.windows.getAll({populate: true}, pass(function(windows) {
      assertEq(1, windows.length);
      normalWindow = windows[0];
      assertTrue(!normalWindow.incognito);
    }));
  },

  function tabEvents() {
    chrome.test.listenOnce(chrome.tabs.onCreated, function(tab) {
      assertTrue(!tab.incognito);
    });

    chrome.test.sendMessage("createIncognitoTab", function(response) {
      if (response == "created") {
        chrome.tabs.create({url: "about:blank"}, pass());
      }
    });
  },
]);
