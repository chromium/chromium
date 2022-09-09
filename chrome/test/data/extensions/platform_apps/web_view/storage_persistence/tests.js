// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Navigates |webview| to |url| and invokes |callback| when the navigation is
// complete.
function navigateWebview(webview, url, callback) {
  webview.onloadstop = function() {
    webview.onloadstop = null;
    callback();
  };
  webview.src = url;
}

// Navigates |webview| to |url| and when the navigation is over, asks |webview|
// to add |cookie|. |callback| is called as soon as |webview| sends an
// acknowledgement for setting its cookies.
function navigateWebviewAndSetCookie(webview, url, cookie, callback) {
  var messageHandler = Messaging.GetHandler();
  // Handles the response to the request for setting the cookie.
  messageHandler.addHandler(SET_COOKIE_COMPLETE, function(message, portFrom) {
    messageHandler.removeHandler(SET_COOKIE_COMPLETE);
    callback();
  });
  // Navigate and when ready, send the request.
  navigateWebview(webview, url, function() {
    messageHandler.sendMessage(
        new Messaging.Message(SET_COOKIE, {'cookie': cookie}),
        webview.contentWindow);
  });
}

// Navigates |webview| to the |url| and when the navigation is over, asks
// |webview| for its cookies. |callback| is invoked as soon as |webview| sends
// back its cookies.
function navigateWebviewAndGetCookies(webview, url, callback) {
  var messageHandler = Messaging.GetHandler();
  // Handles the response to the request for getting cookies.
  messageHandler.addHandler(GET_COOKIES_COMPLETE, function(message, portFrom) {
    messageHandler.removeHandler(GET_COOKIES_COMPLETE);
    callback(message.cookies);
  });
  // Navigate and when ready, send the request.
  navigateWebview(webview, url, function() {
    messageHandler.sendMessage(
        new Messaging.Message(GET_COOKIES, {}),
        webview.contentWindow);
  });
}

// Checks if all the key-value pairs of the first object also exist in the
// second one.
function isFirstObjInSecondObj(a, b) {
  for (var key in a) {
    if (a[key] !== b[key]) {
      return false;
    }
  }
  return true;
}

// Returns true if |a| and |b| are the same.
function isEquivalent(a, b) {
  return isFirstObjInSecondObj(a, b) && isFirstObjInSecondObj(b, a);
}

// Creates a test which verifies if |webview| can correctly set |cookieToSet| as
// a cookie value for |url|.
function createTestToVerifyWebviewSetCookie(webview, url, cookieToSet) {
  return new Testing.Test(webview.id + ' setting cookie ' +
      JSON.stringify(cookieToSet) + ' for ' + url, function(resultCallBack) {
        navigateWebviewAndSetCookie(webview, url, cookieToSet, function() {
          navigateWebviewAndGetCookies(webview, url, function(cookies) {
            resultCallBack(isFirstObjInSecondObj(cookieToSet, cookies));
          });
        });
      });
}

// Creates a test which verifies if a |webview|'s cookie for the |url| matches
// |expectedCookies|.
function createTestToVerifyCookie(webview, url, expectedCookies) {
  return new Testing.Test(
      'check ' + webview.id + ' has cookies ' + JSON.stringify(expectedCookies),
      function (resultCallBack) {
        navigateWebviewAndGetCookies(webview, url, function(cookies) {
          resultCallBack(isEquivalent(cookies, expectedCookies));
        });
      });
}

// Links a list of "Testing.Test" items and links them to each other. Returns
// the first test in the link.
function linkTestsAndReturnFirst(tests) {
  if (tests && tests.length) {
    for (var index = 0; index < tests.length - 1; ++index) {
      tests[index].setNextTest(tests[index + 1]);
    }
    return tests[0];
  }
  return null;
}

// Creates a series of tests which verify correct setting of cookies in
// webviews and also if those webviews on the same partition can see each
// other's cookies.
function createPreStoragePersistenceTests(webviews, url) {
  var tests = [];
  var cookie1 = {'inmemory': 'true'};
  var cookie2 = {'persist1': 'true'};
  var cookie3 = {'persist2': 'true'};
  // In the following, tests are created for each pair of webviews on the same
  // partition. First test verifies setting the cookie in one webview, while the
  // second test verifies if the cookie is observed in the other webview on the
  // same partition

  // Unnamed parition (shared between |webviews[0]| and |webviews[1]|).
  tests.push(createTestToVerifyWebviewSetCookie(webviews[0], url, cookie1));
  tests.push(createTestToVerifyCookie(webviews[1], url, cookie1));

  // Named in-memory parition (shared between |webviews[2]| and |webviews[3]).
  tests.push(createTestToVerifyWebviewSetCookie(webviews[2], url, cookie1));
  tests.push(createTestToVerifyCookie(webviews[3], url, cookie1));

  // Named persistent parition (shared between |webviews[4]| and |webviews[5]|).
  tests.push(createTestToVerifyWebviewSetCookie(webviews[4], url, cookie2));
  tests.push(createTestToVerifyCookie(webviews[5], url, cookie2));

  // Named persistent parition (only for |webviews[6]);
  tests.push(createTestToVerifyWebviewSetCookie(webviews[6], url, cookie3));

  return linkTestsAndReturnFirst(tests);
}

// Creates a set of tests (returns only the first test since they are linked to
// run one after another) aiming to verify the correctness of storage
// persistence for the webviews. This test MUST run after the
// PRE_StoragePersistence tests.
function createStoragePersistenceTests(webviews, url) {
  var tests = [];
  var cookie1 = {};
  var cookie2 = {'persist1': 'true'};
  var cookie3 = {'persist2': 'true'};
  var expectedCookies =
      [cookie1, cookie1, cookie1, cookie1, cookie2, cookie2, cookie3];
  // The first three webviews are not using persistent storage, so their cookies
  // must be empty.
  for (var index = 0; index < expectedCookies.length; ++index) {
    tests.push(
        createTestToVerifyCookie(webviews[index], url, expectedCookies[index]));
  }

  return linkTestsAndReturnFirst(tests);
}

// Handles the |consolemessage| event for a webview.
function onConsoleMessage(e) {
  console.log(this.id + ':' + e.message);
}

// Creates a list of webviews each corresponding to one partition name in
// |partitionNames|, assigns unique |id| to each, listens to the
// |consolemessage| event, and appends them to the |embedder|.
function createWebviews(partitionNames, embedder) {
  var webviews = [];
  for (var index = 0; index < partitionNames.length; ++index) {
    var webview = document.createElement('webview');
    webview.id = 'webview_' + index;
    webview.partition = partitionNames[index];
    webview.onconsolemessage = onConsoleMessage;
    embedder.appendChild(webview);
    webviews.push(webview);
  }
  return webviews;
}

// URL to the guest contents.
function getURL(port) {
  return 'http://localhost:' + port +
      '/extensions/platform_apps/web_view/storage_persistence/guest.html';
}

// Runs the test identified in the custom arguments.
function runTest(port, testName) {
  var url = getURL(port);
  var partitionNames =
      [null, null, 'inmem', 'inmem', 'persist:1', 'persist:1', 'persist:2'];
  var embedder = document.getElementById('embedder');
  var webviews = createWebviews(partitionNames, embedder);
  var test = (testName === 'PRE_StoragePersistence') ?
      createPreStoragePersistenceTests(webviews, url) :
      createStoragePersistenceTests(webviews, url);
  test.run(chrome.test.succeed, chrome.test.fail);
}

chrome.test.getConfig(function(config) {
  var testName = config.customArg;
  runTest(config.testServer.port, testName);
});
