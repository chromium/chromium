// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function changeDocumentTitle() {
  document.title = 'success';
  chrome.test.sendMessage("success");
}

chrome.webNavigation.onCommitted.addListener(async function () {
  let tab = await getCurrentTab();
  await chrome.scripting.executeScript({
    target: { tabId: tab.id },
    function: changeDocumentTitle
  });
});

async function getCurrentTab() {
  let queryOptions = { active: true, currentWindow: true };
  let [tab] = await chrome.tabs.query(queryOptions);
  return tab;
}
