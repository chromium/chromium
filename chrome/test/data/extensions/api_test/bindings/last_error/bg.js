// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

async function testLastError() {
  // Make sure lastError is not yet set
  if (chrome.tabs.lastError)
    throw new Error("Failed");

  var maxTabId = 0;

  // Find the highest tab id
  const windows = await new Promise(resolve => {
    chrome.windows.getAll({populate:true}, resolve);
  });

  // Make sure lastError is still not set. (this call have should succeeded).
  if (chrome.tabs.lastError)
    throw new Error("Failed");

  for (var i = 0; i < windows.length; i++) {
    var win = windows[i];
    for (var j = 0; j < win.tabs.length; j++) {
      const tab = win.tabs[j];
      if (tab.id > maxTabId)
        maxTabId = tab.id;
    }
  }

  // Now ask for the next highest tabId.
  const tab = await new Promise(resolve => {
    chrome.tabs.get(maxTabId + 1, resolve);
  });
  // Make sure lastError *is* set and tab is not.
  if (!chrome.runtime.lastError ||
      !chrome.runtime.lastError.message ||
      tab)
      throw new Error("Failed");

  await new Promise(resolve => {
    window.setTimeout(resolve, 10);
  });

  // Now make sure lastError is unset outside the callback context.
  if (chrome.tabs.lastError)
    throw new Error("Failed");

  return true;
}

chrome.test.sendMessage('ready');
