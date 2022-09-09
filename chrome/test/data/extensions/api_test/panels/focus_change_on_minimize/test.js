// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var waitingForFocus = true;
var panelWinId = chrome.windows.WINDOW_ID_NONE;
var focusedWinId = chrome.windows.WINDOW_ID_NONE;
var listenDoneCallback;

// Focus change event handler to watch for created panel to gain focus.
// Then minimize the panel and wait for it to lose focus by watching
// for an onFocusChanged event with WINDOW_ID_NONE, which is expected
// whenever a panel loses focus.
function onFocusChanged(changedWinId) {
  if (waitingForFocus) {
    if (chrome.windows.WINDOW_ID_NONE != changedWinId) {
      focusedWinId = changedWinId;
      // Only minimize if the focused window is the panel created by
      // this test. Tests might be run in parallel so there might be
      // other focus events that we don't care about.
      if (focusedWinId == panelWinId) {
         minimizePanel();
      }
    }
  } else if (chrome.windows.WINDOW_ID_NONE == changedWinId) {
    listenDoneCallback();
  }
}

// Minimize the created panel after we know it has the focus.
function minimizePanel() {
  chrome.test.assertEq(focusedWinId, panelWinId);
  waitingForFocus = false;
  chrome.windows.update(panelWinId, {'state': 'minimized'},
      chrome.test.callbackPass(function(win) {
          chrome.test.assertEq('minimized', win.state);
      }));
}

// Activate panel so we can minimize it.
function activatePanel() {
  chrome.windows.update(panelWinId, {'focused': true},
      chrome.test.callbackPass(function(win) {
      }));
}

chrome.test.runTests([
  function createPanelToMinimize() {
    listenDoneCallback = chrome.test.listenForever(
        chrome.windows.onFocusChanged, onFocusChanged);
    chrome.windows.create(
        {'url': 'about:blank','type': 'panel'},
        chrome.test.callbackPass(function(win) {
            chrome.test.assertEq('panel', win.type);
            chrome.test.assertEq(true, win.alwaysOnTop);
            panelWinId = win.id;
            activatePanel();
        }));
  }
]);
