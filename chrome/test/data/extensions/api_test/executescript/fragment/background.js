// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var got_request = false;

var test_url = "http://localhost:PORT/extensions/test_file.html";

// For running in normal chrome (ie outside of the browser_tests environment),
// set debug to 1 here.
var debug = 0;
if (debug) {
  test_url = "http://www.google.com/";
  chrome.test.log = function(msg) { console.log(msg) };
  chrome.test.runTests = function(tests) {
    for (var i in tests) {
      tests[i]();
    }
  };
  chrome.test.succeed = function(){ console.log("succeed"); };
  chrome.test.fail = function(){ console.log("fail"); };
}

function navigate_to_fragment(tab, callback) {
  var new_url = test_url + "#foo";
  chrome.test.log("navigating tab to " + new_url);
  chrome.tabs.update(tab.id, {"url": new_url}, callback);
}

var succeeded = false;

function do_execute(tab) {
  chrome.tabs.executeScript(tab.id, {"file": "execute_script.js"});
  setTimeout(function() {
    if (!succeeded) {
      chrome.test.fail("timed out");
    }
  }, 10000);
}

function runTests() {
  chrome.test.runTests([
    // When the tab is created, a content script will send a request letting
    // know the onload has fired. Then we navigate to a fragment, and try
    // running chrome.tabs.executeScript.
    function test1() {
      chrome.runtime.onMessage.addListener(function(req, sender) {
        chrome.test.log("got request: " + JSON.stringify(req));
        if (req == "content_script") {
          navigate_to_fragment(sender.tab, do_execute);
        } else if (req == "execute_script") {
          succeeded = true;
          chrome.test.succeed();
        }
      });
      chrome.test.log("creating tab");
      chrome.tabs.create({"url": test_url});
    }
  ]);
}

if (debug) {
  // No port to fix.  Run tests directly.
  runTests();
} else {
  chrome.test.getConfig(function(config) {
    test_url = test_url.replace(/PORT/, config.testServer.port);
    runTests();
  });
}
