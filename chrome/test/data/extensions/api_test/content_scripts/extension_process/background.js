// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var numPings = 0;
var base_url;

chrome.runtime.onMessage.addListener(function(data) {
  if (data != "ping")
    chrome.test.fail("Unexpected request: " + JSON.stringify(data));

  if (++numPings == 2) {
    // Navigate the iframe to another page and check that the content script is
    // also injected into that page.
    document.querySelector("iframe").src =
        base_url + "test_file_with_body.html";
  } else if (numPings == 3) {
    chrome.test.notifyPass();
  }
});

chrome.test.getConfig(function(config) {
  base_url = "http://localhost:PORT/extensions/"
      .replace(/PORT/, config.testServer.port);
  var test_file_url = base_url + "test_file.html";

  // Add a window.
  var w = window.open(test_file_url);

  // Add an iframe.
  var iframe = document.createElement("iframe");
  iframe.src = test_file_url;
  document.getElementById("iframeContainer").appendChild(iframe);
});
