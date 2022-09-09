// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The main logic of the DOM Storage Isolation test is implemented here.

var FILE_NAME = 'page.html';
var APP_PATH = 'extensions/platform_apps/web_view/dom_storage_isolation';
var HOST_NAME = 'localhost';

function onAllTestsSucceeded() {
  chrome.test.succeed('DOM Storage Isolation');
}

function onTestFailed() {
  chrome.test.fail('DOM Storage Isolation');
}

// Initializes a given webview's DOM storage and invokes the callback
// after.
function askWebviewToInitDOM(webview, name, callback) {
  var messageHandler = new Messaging.Handler();
  // Listen for 'INIT_COMPLETE'
  messageHandler.addHandler(INIT_DOM_STORAGE_COMPLETE, function(message, port) {
    // Remove this handler since it is no longer needed.
    messageHandler.removeHandler(INIT_DOM_STORAGE_COMPLETE);
    callback();
  });
  // Send the request to Init.
  messageHandler.sendMessage(
      new Messaging.Message(INIT_DOM_STORAGE, {pageName: name}),
      webview.contentWindow);
};

// Reads a given webview's DOM storage information and invokes the callback
// by passing the information after.
function askWebviewForStorageInfo(webview, callback) {
  var messageHandler = new Messaging.Handler();
  // Listen for 'GET_INFO_COMPLETE'
  messageHandler.addHandler(GET_DOM_STORAGE_INFO_COMPLETE,
      function(message, port) {
        // Remove this handler since it is no longer needed.
        messageHandler.removeHandler(GET_DOM_STORAGE_INFO_COMPLETE);
        callback(message.local, message.session);
      });
  // Send the request for storage info.
  messageHandler.sendMessage(
      new Messaging.Message(GET_DOM_STORAGE_INFO, {}),
      webview.contentWindow);
}

// Initializes a webview's DOM storage and then reads the storage information
// and verifies if they are what was expected. Then it invokes a callback with
// the result.
function askWebviewToInitDOMAndVerifyResults(webview, name, callback) {
  askWebviewToInitDOM(webview, name, function() {
    askWebviewForStorageInfo(webview, function(local, session) {
      var localOK = (local === ('local-' + name));
      var sessionOK = (session === ('session-' + name));
      callback(localOK, sessionOK);
    });
  });
}

// The test logics are defined here.
function addTests(webviews) {
  // The first test initializes the DOM storage for |webviews[0]| and verifies
  // the correctness of the storage names.
  var test1 = new Testing.Test('init_dom_in_webview[0]',
      function(callBack) {
        askWebviewToInitDOMAndVerifyResults(webviews[0], 'page1',
            function(localOK, sessionOK) {
              callBack(localOK && sessionOK);
            });
      });
  // The second test initializes and verifies the DOM storage for |webviews[1]|
  // and then reads the storage info from |webviews[0]| (on the same parition)
  // to confirm that the local storage name is the same for both.
  // Also note that the session storages should be different.
  var test2 = new Testing.Test(
      'init_dom_in_webview[1]_and_verify_in_webview[0]', function(callBack) {
        // Initialie DOM and verify storage info for |webviews[1]|.
        askWebviewToInitDOMAndVerifyResults(webviews[1], 'page2',
            function(localOK, sessionOK) {
              if (localOK && sessionOK) {
                // Verify the storage info for webviews[0].
                askWebviewForStorageInfo(webviews[0], function(local, session) {
                  if (local !== 'local-page2') {
                    console.log('Error in "' + test2.name + '": bad local ' +
                        'storage name for |webviews[0]|.');
                  } else if (session !== 'session-page1') {
                    console.log('Error in "' + test2.name + '": bad session' +
                        ' storage name for |webviews[0].');
                  } else {
                    return callBack(true);
                  }
                  callBack(false);
                });
              } else {
                console.log('Error in DOM initialization for |webviews[1]|.');
                callBack(false);
              }
            });
      });
  // The third test reads the DOM storage information from |webviews[2]| which
  // is on the default parition and verifies its isolation from |webviews[0]|
  // and |webviews[1]|.
  test3 = new Testing.Test('read_dom_storage_info_from_webviews[2]',
      function(callBack) {
        askWebviewForStorageInfo(webviews[2], function(local, session) {
          if (local !== 'badval') {
            console.log('Error! Local storage is "' + local + '" while it was' +
                ' expected to be "badval".');
          } else if (session !== 'badval') {
            console.log('Error! Session storage is "' + session + '" while it' +
                ' was expected to be "badval".');
          } else {
            return callBack(true);
          }
          callBack(false);
        });
      });
  // Link the tests so that they will run one after another.
  test1.setNextTest(test2);
  test2.setNextTest(test3);
  test3.setNextTest(null); // End of all tests.
  window.firstTest = test1; // The first test to run.
}

function getURL(port) {
  return 'http://' + HOST_NAME  + ':' + port + '/' + APP_PATH + '/' + FILE_NAME;
}

function createWebViews() {
  var webviews = [];
  var container = document.getElementById('container');
  for (var index = 0; index < 3; ++index) {
    webviews.push(document.createElement('webview'));
    webviews[index].id = 'webview_' + index;
    webviews[index].onconsolemessage = function(e) {
      console.log(this.id + ': ' + e.message);
    };
    container.appendChild(webviews[index]);
  }
  // The first two webviews will share a persistent parition.
  webviews[0].partition = 'persist:p';
  webviews[1].partition = 'persist:p';
  return webviews;
}

function loadURLInWebViews(webviews, url, callback) {
  var loadedCount = 0;
  for (var index = 0; index < webviews.length; index++) {
    webviews[index].onloadstop = function() {
      loadedCount++;
      if (loadedCount === webviews.length) {
        callback();
      }
    }
    webviews[index].src = url;
  }
}

function run() {
  var webviews = createWebViews();
  addTests(webviews); // Define tests here.
  chrome.test.getConfig(function(config) {
    loadURLInWebViews(webviews, getURL(config.testServer.port), function() {
      // Start with the first test and the chain of tests will run.
      firstTest.run(onAllTestsSucceeded, onTestFailed);
    });
  });
}
window.onload = run;
