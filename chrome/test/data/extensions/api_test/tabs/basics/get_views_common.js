// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Open a window using chrome.windows.create(options),
// and see that chrome.extension.getViews() can find all the tabs
// in the window by searching for views with the window ID of the
// window we opened.
// Put in a common file because tests that use this function
// are too slow to be run as part of a single browser test.
function testGetNewWindowView(options, expectedViewURLs) {
  chrome.windows.create(
      options, pass(function(win) {
        // Wait for tabs to load, so we can look at window.location.href.
        waitForAllTabs(pass(function() {
          const views = chrome.extension.getViews({windowId: win.id});

          // Build a sorted array of the URLs in |views|.
          const actualUrls = views
                                 .map(function(view) {
                                   return view.location.href;
                                 })
                                 .sort();

          // Make the expected URLs non-relative, and sort them.
          const expectedUrls = expectedViewURLs
                                   .map(function(url) {
                                     return chrome.runtime.getURL(url);
                                   })
                                   .sort();

          // Comparing JSON makes errors easy to read.
          assertEq(
              JSON.stringify(expectedUrls, null, 2),
              JSON.stringify(actualUrls, null, 2));
          chrome.windows.remove(win.id, pass(function() {}));
        }));
      }));
}
