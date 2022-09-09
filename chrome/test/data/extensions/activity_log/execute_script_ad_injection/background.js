// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test that injecting an ad via tabs.executeScript doesn't circumvent
// detection. This is a very simple test, since the methods are tested much
// more extensively in the
// chrome/test/data/extensions/activity_log/ad_injection.
// If this test grows, it could use the same setup as that test, but there's no
// need for that at the time.

var didInject = false;
var code =
    "document.body.appendChild(document.createElement('iframe')).src = " +
        "'http://www.known-ads.adnetwork';";

/**
 * Injects an ad into the tab using chrome.tabs.executeScript().
 * @param {number} tabId The id of the tab to inject into.
 */
function injectScript(tabId) {
  console.log('injectScript');
  if (!didInject) {
    console.log('injecting');
    didInject = true;
    chrome.tabs.executeScript(tabId, {code: code}, function() {
      console.log('injected');
      chrome.test.sendMessage('Done');
    });
  }
}

// Inject the script when the tab is updated.
chrome.tabs.onUpdated.addListener(injectScript);
