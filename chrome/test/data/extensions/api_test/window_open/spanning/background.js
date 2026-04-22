// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let nextTest = null;
openFromNormalShouldOpenInNormal();

function openFromNormalShouldOpenInNormal() {
  nextTest = openFromExtensionHostInIncognitoBrowserShouldOpenInNormalBrowser;
  chrome.windows.getAll({populate: true}, function(windows) {
    chrome.test.assertEq(1, windows.length);
    chrome.test.assertFalse(windows[0].incognito);
    chrome.test.assertEq(1, windows[0].tabs.length);
    chrome.test.assertFalse(windows[0].tabs[0].incognito);

    // A tab works the same as a popup for providing an extension host in the
    // context of this window.
    chrome.tabs.create(
        {windowId: windows[0].id, url: 'popup.html'}, function(tab) {
          chrome.test.assertTrue(!!tab);
          // The rest of the test continues in popup.html.
        });
  });
}

function openFromExtensionHostInIncognitoBrowserShouldOpenInNormalBrowser() {
  nextTest = null;
  chrome.windows.getCurrent(function(normalWin) {
    chrome.test.assertFalse(normalWin.incognito);
    // Create an incognito window.
    chrome.windows.create(
        {
          incognito: true,
          focused: true,
        },
        function(incognitoWin) {
          // Remove the normal window. We keep running because of the incognito
          // window.
          chrome.windows.remove(normalWin.id, function() {
            chrome.tabs.query({windowId: incognitoWin.id}, function(tabs) {
              chrome.test.assertEq(1, tabs.length);
              chrome.tabs.create(
                  {windowId: incognitoWin.id, url: 'popup.html'},
                  function(tab) {
                    chrome.test.assertTrue(!!tab);
                    // The rest of the test continues in popup.html.
                  });
            });
          });
        });
  });
}

function verifyCreatedTab(tab) {
  // The new tab should be a normal tab, and it should be in a normal
  // window.
  chrome.test.assertFalse(tab.incognito);
  chrome.windows.get(tab.windowId, function(win) {
    chrome.test.assertFalse(win.incognito);
    if (nextTest) {
      nextTest();
    } else {
      chrome.test.notifyPass();
    }
  });
}
