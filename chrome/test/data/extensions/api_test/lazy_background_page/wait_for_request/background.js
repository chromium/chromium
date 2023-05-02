// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var testServerHost = "www.a.com";
var testServerPort;
function getServerURL(path) {
  var host = testServerHost;
  return "http://" + host + ":" + testServerPort + "/" + path;
}

var hangingRequest;
function abortRequest() {
  hangingRequest.abort();
  return true;
}

chrome.runtime.onInstalled.addListener(function() {
  chrome.test.getConfig(function(config) {
    testServerPort = config.testServer.port;

    // Start a request that will "never" finish (at least, not for 1000
    // minutes). The browser code will keep us alive until the request is
    // killed.
    hangingRequest = new XMLHttpRequest();
    hangingRequest.onreadystatechange = function() {
      // The request hangs, so this is only ever called once.
      chrome.test.notifyPass();
    }
    hangingRequest.open("GET", getServerURL("slow?60000"), true);
    hangingRequest.send(null);
  });
});
