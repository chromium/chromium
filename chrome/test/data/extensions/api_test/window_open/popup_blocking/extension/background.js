// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function buildUrl(host, port) {
  return ("http://HOST:PORT/extensions/api_test/window_open/" +
          "popup_blocking/extension/foo.html")
             .replace(/HOST/, host).replace(/PORT/, port);
}

chrome.test.getConfig(function(config) {
  var popupURL = buildUrl("a.com", config.testServer.port);
  var webPageURL = buildUrl("b.com", config.testServer.port);

  // Open a popup and a tab from the background page.
  pop(popupURL);

  // Open a popup and a tab from a tab (tabs don't use ExtensionHost, so it's
  // interesting to test them separately).
  chrome.tabs.create({url: "tab.html", index: 1});

  // Open a tab to a URL that will cause our content script to run. The content
  // script will open a popup and a tab.
  chrome.tabs.create({url: webPageURL, index: 2});
});
