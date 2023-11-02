// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The main logic of the Findability Isolation test is implemented here.

var FILE_NAME = 'page.html';
var APP_PATH = 'extensions/platform_apps/web_view/findability_isolation';
var HOST_NAME = 'localhost';

function onAllTestsSucceeded() {
  chrome.test.succeed('Findability Isolation');
}

function onTestFailed() {
  chrome.test.fail('Findability Isolation');
}

// Sets window.name of the given webview.  |callback| will be called with no
// arguments upon completion.
function askWebviewToSetWindowName(webview, name, callback) {
  var messageHandler = new Messaging.Handler();
  // Listen for 'SET_WINDOW_NAME_COMPLETE'
  messageHandler.addHandler(SET_WINDOW_NAME_COMPLETE, function(message, port) {
    // Remove this handler since it is no longer needed.
    messageHandler.removeHandler(SET_WINDOW_NAME_COMPLETE);
    callback();
  });
  // Send the request to set the window name.
  messageHandler.sendMessage(
      new Messaging.Message(SET_WINDOW_NAME, {windowName: name}),
      webview.contentWindow);
}

// Checks if |webview| can find a window/frame named |name|.
// |callback| will be called with a boolean |found| argument upon completion.
function askWebviewToFindWindowByName(webview, name, callback) {
  var messageHandler = new Messaging.Handler();
  // Listen for 'FIND_WINDOW_BY_NAME_COMPLETE'
  messageHandler.addHandler(
      FIND_WINDOW_BY_NAME_COMPLETE,
      function(message, port) {
        // Remove this handler since it is no longer needed.
        messageHandler.removeHandler(FIND_WINDOW_BY_NAME_COMPLETE);
        callback(message.found);
      });
  // Send the request to set the window name.
  messageHandler.sendMessage(
      new Messaging.Message(FIND_WINDOW_BY_NAME, {windowName: name}),
      webview.contentWindow);
}

// The test logics are defined here.
function addTests(webviews) {
  // The first test makes sure that the webviews in the same storage partition
  // (webviews[0] and webviews[1]) can find each other via
  // window.open('', <frame name>).  This is a regression test for
  // https://crbug.com/794079.
  test1 = new Testing.Test('window.open_findability_between_webview[0]and[1]',
      function(callBack) {
        askWebviewToSetWindowName(webviews[1], "test1-webviews1", function() {
          askWebviewToFindWindowByName(
              webviews[0], "test1-webviews1", function(found) {
                // Windows should be findable if they are in the same storage
                // partition.
                callBack(found == true);
              });
        });
      });
  // The second test makes sure that the webviews in a different storage
  // partition (webviews[0] and webviews[2]) can NOT find each other via
  // window.open('', <frame name>).
  test2 = new Testing.Test('window.open_findability_between_webview[0]and[2]',
      function(callBack) {
        askWebviewToSetWindowName(webviews[2], "test2-webviews2", function() {
          askWebviewToFindWindowByName(
              webviews[0], "test2-webviews2", function(found) {
                // Windows shouldn't be findable across different storage
                // partitions.
                callBack(found == false);
              });
        });
      });
  // Link the tests so that they will run one after another.
  test1.setNextTest(test2);
  test2.setNextTest(null); // End of all tests.
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
