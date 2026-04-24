// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let failed = false;

function didFail() {
  return failed;
}

function setFailed(val) {
  failed = val;
}

function fail() {
  setFailed(true);
  if (!didFail()) {
    chrome.test.fail();
  }
}

let testUrl = '';

// For running in normal chrome (ie outside of the browser_tests environment),
// set debug to 1 here.
const debug = 0;
if (debug) {
  testUrl = 'http://www.google.com';
  chrome.test.log = function(msg) {
    console.log(msg);
  };
  chrome.test.runTests = function(tests) {
    for (const i in tests) {
      tests[i]();
    }
  };
  chrome.test.succeed = function() {
    console.log('succeed');
  };
  chrome.test.fail = function() {
    console.log('fail');
  };
}

function runTests() {
  chrome.test.runTests([function test1() {
    chrome.runtime.onMessage.addListener(function(req, sender) {
      chrome.test.log(`got request: ${JSON.stringify(req)}`);
      if (req == 'fail') {
        fail();
      } else if (req == 'content_script_start') {
        const tab = sender.tab;
        if (tab.url.indexOf('#') != -1) {
          fail();
        } else {
          chrome.tabs.update(tab.id, {url: `${tab.url}#foo`});
        }
      }
    });
    chrome.tabs.onUpdated.addListener(function(tabid, info, tab) {
      chrome.test.log(`onUpdated status: ${info.status} url:${tab.url}`);
      if (info.status == 'complete' && tab.url.indexOf('#foo') != -1) {
        setTimeout(function() {
          if (!didFail()) {
            chrome.test.succeed();
          }
        }, 750);
      }
    });
    chrome.test.log('creating tab');
    chrome.tabs.create({url: testUrl});
  }]);
}

if (debug) {
  runTests();
} else {
  chrome.test.getConfig(function(config) {
    testUrl = `http://localhost:${config.testServer.port}` +
        '/extensions/test_file.html';
    runTests();
  });
}
