// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var failed = false;

function did_fail() {
  return failed;
}

function set_failed(val) {
  failed = val;
}

function fail() {
  set_failed(true);
  if (!did_fail()) {
    chrome.test.fail();
  }
}

var test_url = "http://localhost:PORT/extensions/test_file.html";

// For running in normal chrome (ie outside of the browser_tests environment),
// set debug to 1 here.
var debug = 0;
if (debug) {
  test_url = "http://www.google.com";
  chrome.test.log = function(msg) { console.log(msg) };
  chrome.test.runTests = function(tests) {
    for (var i in tests) {
      tests[i]();
    }
  };
  chrome.test.succeed = function(){ console.log("succeed"); };
  chrome.test.fail = function(){ console.log("fail"); };
}

function runTests() {
  chrome.test.runTests([
    function test1() {
      chrome.runtime.onMessage.addListener(function(req, sender) {
        chrome.test.log("got request: " + JSON.stringify(req));
        if (req == "fail") {
          fail();
        } else if (req == "content_script_start") {
          var tab = sender.tab;
          if (tab.url.indexOf("#") != -1) {
            fail();
          } else {
            chrome.tabs.update(tab.id, {"url": tab.url + "#foo"});
          }
        }
      });
      chrome.tabs.onUpdated.addListener(function(tabid, info, tab) {
        chrome.test.log("onUpdated status: " + info.status + " url:" + tab.url);
        if (info.status == "complete" && tab.url.indexOf("#foo") != -1) {
          setTimeout(function() {
            if (!did_fail()) {
              chrome.test.succeed();
            }
          }, 750);
        }
      });
      chrome.test.log("creating tab");
      chrome.tabs.create({"url": test_url});
    }
  ]);
}

if (debug) {
  runTests();
} else {
  chrome.test.getConfig(function(config) {
    test_url = test_url.replace(/PORT/, config.testServer.port);
    runTests();
  });
}
