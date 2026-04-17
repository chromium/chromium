// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function changeDocumentTitle() {
  document.title = 'success';
  chrome.test.sendMessage('injection succeeded');
}

chrome.webNavigation.onCommitted.addListener(async function(activeInfo) {
  const tab = await getCurrentTab();
  await chrome.scripting.executeScript({
    target: {tabId: tab.id},
    function: changeDocumentTitle,
  });
});

async function getCurrentTab() {
  const queryOptions = {active: true, currentWindow: true};
  const [tab] = await chrome.tabs.query(queryOptions);
  return tab;
}
