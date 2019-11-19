// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Navigates to |url| and invokes |callback| when the navigation is complete.
function navigateTab(url, callback) {
  chrome.tabs.onUpdated.addListener(function updateCallback(_, info, tab) {
    if (info.status == 'complete' && tab.url == url) {
      chrome.tabs.onUpdated.removeListener(updateCallback);
      callback(tab);
    }
  });

  chrome.tabs.update({url: url});
}

var testServerPort;
var host = 'xyz.com';
function getServerURL(path) {
  if (!testServerPort)
    throw new Error('Called getServerURL outside of runTests.');
  return `http://${host}:${testServerPort}/${path}`
}

// Returns whether |headerName| is present in |headers|.
function checkHasHeader(headers, headerName) {
  return !!headers.find(header => header.name.toLowerCase() == headerName);
}

// Adds or updates the given header name/value to |headers|.
function addOrUpdateHeader(headers, headerName, headerValue) {
  var index =
      headers.findIndex(header => header.name.toLowerCase() == headerName);
  if (index != -1) {
    headers[index].value = headerValue;
  } else {
    headers.push({name: headerName, value: headerValue});
  }
}

// Checks whether the cookie request header was removed from the request and
// that it isn't visible to web request listeners. Then proceeds to the next
// test.
function checkCookieHeaderRemoved(expectRemoved) {
  var echoCookieUrl = getServerURL('echoheader?cookie');

  // Register web request listeners for |echoCookieUrl|.
  var filter = {urls: [echoCookieUrl]};
  var extraInfoSpec = ['requestHeaders', 'extraHeaders'];
  var onBeforeSendHeadersSeen = false;
  chrome.webRequest.onBeforeSendHeaders.addListener(function listener(details) {
    chrome.webRequest.onBeforeSendHeaders.removeListener(listener);
    onBeforeSendHeadersSeen = true;
    chrome.test.assertEq(
        !expectRemoved, checkHasHeader(details.requestHeaders, 'cookie'));
  }, filter, extraInfoSpec);

  var onSendHeadersSeen = false;
  chrome.webRequest.onSendHeaders.addListener(function listener(details) {
    chrome.webRequest.onSendHeaders.removeListener(listener);
    onSendHeadersSeen = true;
    chrome.test.assertEq(
        !expectRemoved, checkHasHeader(details.requestHeaders, 'cookie'));
  }, filter, extraInfoSpec);

  navigateTab(echoCookieUrl, function(tab) {
    chrome.test.assertTrue(onBeforeSendHeadersSeen);
    chrome.test.assertTrue(onSendHeadersSeen);
    chrome.tabs.executeScript(
        tab.id, {code: 'document.body.innerText'}, function(results) {
          chrome.test.assertNoLastError();
          chrome.test.assertEq(
              expectRemoved ? 'None' : 'foo1=bar1; foo2=bar2', results[0]);
          chrome.test.succeed();
        });
  });
}

// Removes all the cookies and optionally checks if |optCurrentCookiesSet|
// corresponds to the current cookies. Returns a promise.
function checkAndResetCookies(optCurrentCookiesSet) {
  var removeCookiesPromise =
      function(cookieParams) {
    return new Promise((resolve, reject) => {
      chrome.cookies.remove(cookieParams, function(details) {
        chrome.test.assertNoLastError();
        resolve();
      });
    });
  }

  var url = getServerURL('');

  return new Promise((resolve, reject) => {
    chrome.cookies.getAll({url: url}, function(cookies) {
      if (optCurrentCookiesSet) {
        chrome.test.assertEq(cookies.length, optCurrentCookiesSet.size);
        for (var i = 0; i < cookies.length; ++i)
          chrome.test.assertTrue(optCurrentCookiesSet.has(cookies[i].name));
      }

      var promises = [];
      for (var i = 0; i < cookies.length; ++i)
        promises.push(removeCookiesPromise({url: url, name: cookies[i].name}));

      Promise.all(promises).then(resolve, reject);
    });
  });
}

// Checks whether the set-cookie response header was removed from the request
// and that it isn't visible to web request listeners. Then proceeds to the next
// test.
function checkSetCookieHeaderRemoved(expectRemoved) {
  var setCookieUrl = getServerURL('set-cookie?foo1=bar1&foo2=bar2');

  // Register web request listeners for |setCookieUrl|.
  var filter = {urls: [setCookieUrl]};
  var extraInfoSpec = ['responseHeaders', 'extraHeaders'];
  var onHeadersReceivedSeen = false;
  chrome.webRequest.onHeadersReceived.addListener(function listener(details) {
    chrome.webRequest.onHeadersReceived.removeListener(listener);
    onHeadersReceivedSeen = true;
    chrome.test.assertEq(
        !expectRemoved, checkHasHeader(details.responseHeaders, 'set-cookie'));
  }, filter, extraInfoSpec);

  var onResponseStartedSeen = false;
  chrome.webRequest.onResponseStarted.addListener(function listener(details) {
    chrome.webRequest.onResponseStarted.removeListener(listener);
    onResponseStartedSeen = true;
    chrome.test.assertEq(
        !expectRemoved, checkHasHeader(details.responseHeaders, 'set-cookie'));
  }, filter, extraInfoSpec);


  // Clear cookies from existing tests.
  checkAndResetCookies().then(function() {
    navigateTab(setCookieUrl, function(tab) {
      chrome.test.assertTrue(onHeadersReceivedSeen);
      chrome.test.assertTrue(onResponseStartedSeen);

      var expectedCookies = expectRemoved ? [] : ['foo1', 'foo2'];
      checkAndResetCookies(new Set(expectedCookies)).then(chrome.test.succeed);
    });
  });
}

// Checks whether the cookie request header added by Web request extension was
// removed. Then proceeds to the next test.
function checkAddWebRequestCookie(expectRemoved) {
  var echoCookieUrl = getServerURL('echoheader?cookie');

  // Register web request listeners for |echoCookieUrl|.
  var filter = {urls: [echoCookieUrl]};
  var extraInfoSpec = ['requestHeaders', 'extraHeaders', 'blocking'];
  var onBeforeSendHeadersSeen = false;
  var onBeforeSendHeadersListener = function listener(details) {
    onBeforeSendHeadersSeen = true;
    addOrUpdateHeader(details.requestHeaders, 'cookie', 'webRequest=true');
    return {requestHeaders: details.requestHeaders};
  };
  chrome.webRequest.onBeforeSendHeaders.addListener(
      onBeforeSendHeadersListener, filter, extraInfoSpec);

  var onActionIgnoredCalled = false;
  var onActionIgnoredListener = function(details) {
    onActionIgnoredCalled = true;
    chrome.test.assertEq('request_headers', details.action);
  };
  chrome.webRequest.onActionIgnored.addListener(onActionIgnoredListener);

  navigateTab(echoCookieUrl, function(tab) {
    chrome.webRequest.onBeforeSendHeaders.removeListener(
        onBeforeSendHeadersListener);
    chrome.webRequest.onActionIgnored.removeListener(onActionIgnoredListener);

    chrome.test.assertTrue(onBeforeSendHeadersSeen);
    chrome.test.assertEq(expectRemoved, onActionIgnoredCalled);

    chrome.tabs.executeScript(
        tab.id, {code: 'document.body.innerText'}, function(results) {
          chrome.test.assertNoLastError();
          chrome.test.assertEq(
              expectRemoved ? 'None' : 'webRequest=true', results[0]);
          chrome.test.succeed();
        });
  });
}

// Checks whether the set-cookie request header added by Web request extension
// was removed.
function checkAddWebRequestSetCookie(expectRemoved) {
  var url = getServerURL('echo');

  // Register web request listeners for |url|.
  var filter = {urls: [url]};
  var extraInfoSpec = ['responseHeaders', 'extraHeaders', 'blocking'];
  var onHeadersReceivedSeen = false;
  var onHeadersReceivedListener = function listener(details) {
    onHeadersReceivedSeen = true;
    addOrUpdateHeader(details.responseHeaders, 'set-cookie', 'webRequest=true');
    return {responseHeaders: details.responseHeaders};
  };
  chrome.webRequest.onHeadersReceived.addListener(
      onHeadersReceivedListener, filter, extraInfoSpec);

  var onActionIgnoredCalled = false;
  var onActionIgnoredListener = function(details) {
    onActionIgnoredCalled = true;
    chrome.test.assertEq('response_headers', details.action);
  };
  chrome.webRequest.onActionIgnored.addListener(onActionIgnoredListener);

  checkAndResetCookies().then(function() {
    navigateTab(url, function(tab) {
      chrome.webRequest.onHeadersReceived.removeListener(
          onHeadersReceivedListener);
      chrome.webRequest.onActionIgnored.removeListener(onActionIgnoredListener);

      chrome.test.assertTrue(onHeadersReceivedSeen);
      chrome.test.assertEq(expectRemoved, onActionIgnoredCalled);

      var expectedCookies = expectRemoved ? [] : ['webRequest']
      checkAndResetCookies(new Set(expectedCookies)).then(chrome.test.succeed);
    });
  });
}

// Clears the current state by removing rules specified in |ruleIds| and
// clearing all cookies.
function clearState(ruleIds, callback) {
  chrome.declarativeNetRequest.updateDynamicRules(ruleIds, [], function() {
    chrome.test.assertNoLastError();
    checkAndResetCookies().then(callback);
  });
}

var removeCookieRule = {
  id: 1,
  condition: {urlFilter: host, resourceTypes: ['main_frame']},
  action: {type: 'removeHeaders', removeHeadersList: ['cookie']}
};
var removeSetCookieRule = {
  id: 2,
  condition: {urlFilter: host, resourceTypes: ['main_frame']},
  action: {type: 'removeHeaders', removeHeadersList: ['setCookie']}
};
var allowRule = {
  id: 3,
  condition: {urlFilter: host, resourceTypes: ['main_frame']},
  action: {type: 'allow'}
};

var tests = [
  function testCookieWithoutRules() {
    navigateTab(getServerURL('set-cookie?foo1=bar1&foo2=bar2'), function() {
      checkCookieHeaderRemoved(false);
    });
  },

  function addRulesAndTestCookieRemoval() {
    var rules = [removeCookieRule];
    chrome.declarativeNetRequest.updateDynamicRules([], rules, function() {
      chrome.test.assertNoLastError();
      checkCookieHeaderRemoved(true);
    });
  },

  function testSetCookieWithoutRules() {
    checkSetCookieHeaderRemoved(false);
  },

  function addRulesAndTestSetCookieRemoval() {
    var rules = [removeSetCookieRule];
    chrome.declarativeNetRequest.updateDynamicRules([], rules, function() {
      chrome.test.assertNoLastError();
      checkSetCookieHeaderRemoved(true);
    });
  },

  function testAddWebRequestCookie() {
    // First clear the rules and cookies.
    clearState([1, 2], () => {
      checkAddWebRequestCookie(false);
    });
  },

  function testAddWebRequestCookieWithRules() {
    var rules = [removeCookieRule];
    chrome.declarativeNetRequest.updateDynamicRules([], rules, function() {
      checkAddWebRequestCookie(true);
    });
  },

  function testAddWebRequestSetCookie() {
    checkAddWebRequestSetCookie(false);
  },

  function testAddWebRequestSetCookieWithRules() {
    var rules = [removeSetCookieRule];
    chrome.declarativeNetRequest.updateDynamicRules([], rules, function() {
      checkAddWebRequestSetCookie(true);
    });
  },

  function testAddWebRequestCookieWithAllowRule() {
    chrome.declarativeNetRequest.updateDynamicRules([], [allowRule], () => {
       checkAddWebRequestSetCookie(false);
    });
  },
];

chrome.test.getConfig(function(config) {
  testServerPort = config.testServer.port;
  chrome.test.runTests(tests);
});
