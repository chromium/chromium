// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var results = {
  filtered: {},
  unfiltered: {}
};

function recordEvents(category, name, windowId, windowType) {
  if (!results[category][windowId])
    results[category][windowId] = { id: windowId, type: windowType };
  results[category][windowId][name] = true
}

chrome.windows.onCreated.addListener(function(win) {
  recordEvents('filtered', 'create', win.id, win.type);
});
chrome.windows.onRemoved.addListener(function(id) {
  recordEvents('filtered', 'remove', id, null);
});
chrome.windows.onFocusChanged.addListener(function(id) {
  recordEvents('filtered', 'focus', id, null);
});

var noFilter = { windowTypes: ['app', 'devtools', 'normal', 'panel', 'popup'] };
chrome.windows.onCreated.addListener(function(win) {
  recordEvents('unfiltered', 'create', win.id, win.type);
}, noFilter);
chrome.windows.onRemoved.addListener(function(id) {
  recordEvents('unfiltered', 'remove', id, null);
}, noFilter);
chrome.windows.onFocusChanged.addListener(function(id) {
  recordEvents('unfiltered', 'focus', id, null);
}, noFilter);

chrome.test.sendMessage('ready', function (message) {
  chrome.windows.getCurrent(function(currentWindow) {
    var filteredCount = 0;
    for (var i in results.filtered) {
      var win = results.filtered[i];
      if (win.id == currentWindow.id || win.id == -1)
        continue;
      filteredCount++;
      chrome.test.assertFalse(win.type == 'app' || win.type == 'devtools',
                              'Unexpected window type "' +
                              win.type + '" in filtered events');
      chrome.test.assertTrue(win.create == true,
                             'Missing create event for ' + win.type);
      chrome.test.assertTrue(win.remove == true,
                             'Missing remove event for ' + win.type);
      chrome.test.assertTrue(win.focus == true,
                             'Missing focus event for ' + win.type);
    }
    chrome.test.assertEq(1, filteredCount);

    var unfilteredCount = 0;
    var includes_app = false, includes_devtools = false;
    for (var i in results.unfiltered) {
      var win = results.unfiltered[i];
      if (win.id == currentWindow.id || win.id == -1)
        continue;
      unfilteredCount++;
      if (win.type == 'app')
        includes_app = true;
      if (win.type == 'devtools')
        includes_devtools = true;
      chrome.test.assertTrue(win.create == true,
                             'Missing create event for ' + win.type);
      chrome.test.assertTrue(win.remove == true,
                             'Missing remove event for ' + win.type);
      if (message == 'focus')
        chrome.test.assertTrue(win.focus == true,
                               'Missing focus event for ' + win.type);
    }
    chrome.test.assertEq(2, unfilteredCount);
    chrome.test.assertFalse(
        includes_app,
        'Should not include windows for a separate platform app.');
    chrome.test.assertTrue(includes_devtools,
                           'Could not find app or devtools windows');

    chrome.test.notifyPass();
  });
});
