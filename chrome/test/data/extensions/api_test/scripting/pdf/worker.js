// Copyright 2024 The Chromium Authors
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
  async function injectingInPdfContentFramesIsDisallowed() {
    const query = {url: 'http://a.com/*'};
    let tab = await getSingleTab(query);
    // There should be exactly two frames, which are the main frame and the PDF
    // embed frame.
    let frames = await chrome.webNavigation.getAllFrames({tabId: tab.id});
    chrome.test.assertEq(2, frames.length);

    // There should be exactly two results from executeScript.
    let results = await chrome.scripting.executeScript({
      target: {
        tabId: tab.id,
        allFrames: true,
      },
      func: injectedFunction,
    });

    // Only two frames should execute the script, which are the main frame and
    // the PDF embed frame. The PDF extension frame and the inner embed frame
    // should not execute the script.
    chrome.test.assertEq(2, results.length);
    chrome.test.assertEq('/page_with_embedded_pdf.html', results[0].result);
    chrome.test.assertEq('/pdf/test.pdf', results[1].result);
    chrome.test.succeed();
  },
]);
