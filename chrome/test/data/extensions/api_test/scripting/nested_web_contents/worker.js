// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function injectedFunction() {
  return location.pathname;
}

// Returns the single tab matching the given `query`.
async function getSingleTab(query) {
  const tabs = await chrome.tabs.query(query);
  chrome.test.assertEq(1, tabs.length);
  return tabs[0];
}

chrome.test.runTests([
  async function nestedWebContents() {
    const query = {url: 'http://a.com/*'};
    let tab = await getSingleTab(query);
    // There should be exactly 2 frames.
    let frames = await chrome.webNavigation.getAllFrames({tabId: tab.id});
    chrome.test.assertEq(2, frames.length);

    // There should be exactly 2 results from executeScript.
    let results = await chrome.scripting.executeScript({
      target: {
        tabId: tab.id,
        allFrames: true,
      },
      func: injectedFunction,
    });

    // We see two frames here, the main frame and one for the embed. We should
    // *not* see the third "embed within the embed" created by the PDF
    // reader.
    chrome.test.assertEq(2, results.length);
    chrome.test.assertEq('/page_with_embedded_pdf.html', results[0].result);
    chrome.test.assertEq('/pdf/test.pdf', results[1].result);
    chrome.test.succeed();
  },
]);
