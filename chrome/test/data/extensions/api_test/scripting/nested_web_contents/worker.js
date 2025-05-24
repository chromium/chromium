// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getSingleTab} from '/_test_resources/test_util/tabs_util.js';

function injectedFunction() {
  return location.pathname;
}

chrome.test.runTests([
  async function nestedWebContents() {
    const query = {url: 'http://a.com/*'};
    let tab = await getSingleTab(query);
    // There should be exactly one frame, which is the main frame. The frame for
    // the nested WebContents should not be included here.
    let frames = await chrome.webNavigation.getAllFrames({tabId: tab.id});
    chrome.test.assertEq(1, frames.length);

    // There should be exactly one result from executeScript.
    let results = await chrome.scripting.executeScript({
      target: {
        tabId: tab.id,
        allFrames: true,
      },
      func: injectedFunction,
    });

    // Only one frame should execute the script, which is the main frame. The
    // frame for the nested WebContents should not execute the script.
    chrome.test.assertEq(1, results.length);
    chrome.test.assertEq('/iframe_about_blank.html', results[0].result);
    chrome.test.succeed();
  },
]);
