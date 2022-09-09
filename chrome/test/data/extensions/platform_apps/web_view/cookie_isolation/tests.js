// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var HOST_NAME = 'localhost';
var APP_PATH = 'extensions/platform_apps/web_view/cookie_isolation';
var FILE_NAME = 'page.html';
var webviews = [];
var TEST_FAILED = false; // Result of a test that failed.
var TEST_SUCCEEDED = true; // Result of a test that succeeded.

function getURL(port) {
  return 'http://' + HOST_NAME + ':' + port + '/' + APP_PATH + '/' + FILE_NAME;
}

function tomorrow() {
  var date = new Date();
  date.setDate(date.getDate() + 1);
  return date;
}

// Announces the test result to the test manager.
function announceTestEnd(testName, testResult) {
  var messageHandler = Messaging.GetHandler();
  messageHandler.sendMessage(
      new Messaging.Message(
          testName, 'test_manager', {type: TEST_ENDED, result: testResult}),
      window);
}

// Send a request on behalf of 'test_manager' to a given test and asks it to
// start their test.
function requestTestStart(testName) {
  var messageHandler = Messaging.GetHandler();
  messageHandler.sendMessage(
      new Messaging.Message('test_manager', testName, {type: START_TEST}),
      window);
}

// Tests that webview and the embedder app do not share cookie.
// Note: Before running this chrome app, the test has written
// a sample cookie: "testCookie=1" on a browser page.
function createTestWebviewDoesNotSeeBrowserTab() {
  var agent = new Messaging.Agent('first_test');
  var messageHandler = Messaging.GetHandler();

  // Verifies that |webviews[0]| does not see the browser cookies.
  agent.addTask(START_TEST, function(message, portFrom) {
    // Send a message to |webviews[0]| to receive their cookie.
    console.log('Asking |webviews[0]| for their cookies.');
    messageHandler.sendMessage(
        new Messaging.Message(
            agent.getName(), 'agent_cookie', {type: GET_COOKIES}),
        webviews[0].contentWindow);
  });

  // Handles the message from |webviews[0]| with its cookies.
  agent.addTask(GET_COOKIES_COMPLETE, function(message, portFrom) {
    // |webviews[0]|'s cookie should NOT include "testCookie=1".
    var testResult = (message.content.cookies.hasOwnProperty('testCookie')) ?
        TEST_FAILED : TEST_SUCCEEDED;
    console.log('Read the cookies from |webviews[0]|.');
    announceTestEnd(agent.getName(), testResult);
  });
  // End of the first test.

  messageHandler.addAgent(agent);
};

// Tests that the two webviews on the same in-memory partition share cookies.
function createTestForWebviewsOnSamePartition() {
  var agent = new Messaging.Agent('second_test');
  var messageHandler = Messaging.GetHandler();

  // Requests |webviews[0]| and |webviews[1]| to delete all their cookies.
  agent.addTask(START_TEST, function(message, portFrom) {
    for (var i = 0; i < 2; ++i) {
      messageHandler.sendMessage(
          new Messaging.Message(
              agent.getName(), 'agent_cookie', {type: CLEAR_COOKIES}),
          webviews[i].contentWindow);
    }
  });

  // Handles the reponse from |webviews[0]| and |webviews[1]| regarding their
  // cookies being deleted.
  var numWebviewsClearedCookies = 0;
  agent.addTask(CLEAR_COOKIES_COMPLETE, function(message, portFrom) {
    // A webview cleared its cookies. Keeping track of their number.
    numWebviewsClearedCookies++;
    // Create a unique cookie for this webview.
    var cookieName = 'guest' + numWebviewsClearedCookies;
    // Request the webview to write their cookie.
    messageHandler.sendMessage(
        new Messaging.Message( agent.getName(), 'agent_cookie', {
          type: SET_COOKIES,
          cookieData: new Cookie.CookieData(cookieName, 'true', '/',
              tomorrow())
        }),
        portFrom);
  });

  // Handles the response from |webviews[0]| and |webviews[1]| acknowledging
  // the cookie writing task.
  var numCookiesWritten = 0;
  agent.addTask(SET_COOKIES_COMPLETE, function(message, portFrom) {
    numCookiesWritten++; // Keeping track of how many webviews finished writing.
    if (numCookiesWritten == 2) {
      // Both finished writing. Ask for their cookie value.
      for (var i = 0; i < 2; ++i) {
        console.log('Both webviews finished writing their cookies.' +
            ' Asking them to read us their cookie now.');
        messageHandler.sendMessage(
            new Messaging.Message(
                agent.getName(), 'agent_cookie', {type: GET_COOKIES}),
            webviews[i].contentWindow);
      }
    }
  });

  // Handles the response from webviews reporting their cookie value.
  var numCookiesRead = 0;
  var wrongCookieCount = false;
  agent.addTask(GET_COOKIES_COMPLETE, function(message, portFrom) {
    numCookiesRead++; // Keeping track of how many webviews have reported.
    if (wrongCookieCount) {
      // Already failed the test.
      return;
    }
    console.log('Some webview sent us its cookie(s).');
    console.log('Cookie: ' + JSON.stringify(message.content.cookies) + '.');
    var count = Object.keys(message.content.cookies).length;
    // Expecting two cookies. If less or more, test fails.
    if (count !== 2) {
      wrongCookieCount = true;
      announceTestEnd(agent.getName(), TEST_FAILED);
      return;
    }
    console.log("Read exactly two cookies.");
    if (numCookiesRead == 2) {
      // Read from both webviews with no errors.
      // Test is over.
      announceTestEnd(agent.getName(), TEST_SUCCEEDED);
    }

  });
  // End of the second Test.

  messageHandler.addAgent(agent);
}

// Tests if the third webview which is on a separate partition has empty cookie.
function createTestForWebviewOnDifferentParition() {
  var agent = new Messaging.Agent('third_test');
  var messageHandler = Messaging.GetHandler();

  // Handles the test start message.
  agent.addTask(START_TEST, function(message, portFrom) {
    console.log('Asking the webview on a different partition to send us' +
        ' its cookies.');
    // Ask |webviews[2]| to report its cookies.
    messageHandler.sendMessage(
        new Messaging.Message(
            agent.getName(), 'agent_cookie', {type: GET_COOKIES}),
        webviews[2].contentWindow);
  });

  // Handles the response from |webviews[2]| regarding the value of its cookies.
  agent.addTask(GET_COOKIES_COMPLETE,  function(message, portFrom) {
    console.log('The webview on a different partition sent us its' +
        ' cookies.');
    var count = Object.keys(message.content.cookies).length;
    var result = (count === 0);
    // Test succeeds only if no cookies are reported.
    announceTestEnd(agent.getName(), result);
  });
  // End of the third test.

  messageHandler.addAgent(agent);
}

// Creates an agent which manages the tests.
function createTestManager() {
  var agent = new Messaging.Agent('test_manager');
  var messageHandler = Messaging.GetHandler();

  // Handles the repsonse from test agents which have finished their test.
  agent.addTask(TEST_ENDED, function(message, portFrom) {
    // Early return if test failed.
    if (message.result === TEST_FAILED) {
      chrome.test.failed();
      return;
    }
    switch (message.source) {
      case 'first_test':
        console.log('First test ended.');
        requestTestStart('second_test');
        break;
      case 'second_test':
        console.log('Second test ended.');
        requestTestStart('third_test');
        break;
      case 'third_test':
        console.log('Third test ended.');
        chrome.test.succeed();
        break;
    }
  });

  messageHandler.addAgent(agent);
}

function loadContentIntoWebviews(webviews, url, onLoadCallBack) {
  var loadedWebviewsCount = 0;
  for (var index = 0; index < webviews.length; ++index) {
    webviews[index].onloadstop = function() {
      loadedWebviewsCount++;
      if (loadedWebviewsCount === webviews.length) {
        onLoadCallBack();
      }
    };
    webviews[index].src = url;
  }
}

// Defines the tests agents and implements the test logic.
function createTestAgents() {
  createTestWebviewDoesNotSeeBrowserTab();
  createTestForWebviewsOnSamePartition();
  createTestForWebviewOnDifferentParition();
  createTestManager();
}

function createWebViews() {
  var webviews = [];
  var container = document.getElementById('container');
  for (var i = 0; i < 3; ++i) {
    webviews.push(document.createElement('webview'));
    webviews[i].id = 'webview_' + i;
    container.appendChild(webviews[i]);
    webviews[i].onconsolemessage = function(e) {
      console.log(this.id + ': ' + e.message);
    };
  }
  // Put the last webview on a unique separate partition
  webviews[2].partition = 'persist:p3';
  return webviews;
}

// Creates the required DOM elements and runs the tests.
function run() {
  createTestAgents();
  window.webviews = createWebViews();
  chrome.test.getConfig(function(config) {
    loadContentIntoWebviews(webviews, getURL(config.testServer.port),
        function() {
          // Start the first test.
          requestTestStart('first_test');
        });
  });
}
window.onload = run;
