// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function injectedFunction() {
  return location.href;
}

async function getSingleTab(query) {
  const tabs = await new Promise(resolve => {
    chrome.tabs.query(query, resolve);
  });
  chrome.test.assertEq(1, tabs.length);
  return tabs[0];
}

chrome.test.runTests([
  async function allowedTopFrameAccess() {
    const query = {url: 'http://a.com/*'};
    let tab = await getSingleTab(query);
    const results = await new Promise(resolve => {
      chrome.scripting.executeScript(
          {
            target: {
              tabId: tab.id,
              allFrames: true,
            },
            function: injectedFunction,
          },
          resolve);
    });
    chrome.test.assertNoLastError();
    chrome.test.assertEq(2, results.length);

    // Note: The 'a.com' result is guaranteed to be first, since it's the root
    // frame.
    const url1 = new URL(results[0].result);
    chrome.test.assertEq('a.com', url1.hostname);

    const url2 = new URL(results[1].result);
    chrome.test.assertEq('b.com', url2.hostname);
    chrome.test.succeed();
  },

  async function disallowedTopFrameAccess() {
    const query = {url: 'http://d.com/*'};
    let tab = await getSingleTab(query);
    const results = await new Promise(resolve => {
      chrome.scripting.executeScript(
          {
            target: {
              tabId: tab.id,
              allFrames: true,
            },
            function: injectedFunction,
          },
          resolve);
    });
    chrome.test.assertNoLastError();

    // The extension doesn't have access to the top frame (d.com), but does to
    // one of the subframes (b.com). This injection should succeed (leading to a
    // single result).
    chrome.test.assertEq(1, results.length);
    const url1 = new URL(results[0].result);
    chrome.test.assertEq('b.com', url1.hostname);
    chrome.test.succeed();
  },
]);
