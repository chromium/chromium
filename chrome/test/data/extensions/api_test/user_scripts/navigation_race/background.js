// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.getConfig(async function(config) {
  const url_a = await chrome.test.sendMessage('ready');
  const tab = await chrome.tabs.create({url: url_a});
  await chrome.test.sendMessage('tab_created');

  // C++ replies when the C++ side has blocked the file task runner.
  // We now call execute.
  const promise = chrome.userScripts.execute({
    target: {tabId: tab.id},
    js: [{file: 'script.js'}],
  });

  // Let C++ know we called execute so it can navigate the tab.
  await chrome.test.sendMessage('execute_called');

  try {
    await promise;
    chrome.test.fail('Execution succeeded incorrectly');
  } catch (e) {
    // Because the extension doesn't have the 'tabs' permission, the
    // browser-side check generates a generic error rather than leaking the
    // cross-origin URL.
    if (e.message.includes('Cannot access contents of the page')) {
      chrome.test.succeed();
    } else {
      chrome.test.fail('Unexpected error: ' + e.message);
    }
  }
});
