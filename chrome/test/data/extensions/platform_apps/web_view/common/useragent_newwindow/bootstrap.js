// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

config.IS_CHROME_TEST = true;
// No TestServer.
config.IS_JS_ONLY_GUEST = true;
config.TEST_DIR = 'useragent_newwindow';
var USER_AGENT_OVERRIDE = 'Mozilla/5.0 (X11; U; Linux x86_64; en-US) ' +
    'AppleWebKit/540.0 (KHTML,like Gecko) Chrome/9.1.0.0 Safari/540.0';
var ANDROID_USER_AGENT = 'Mozilla/5.0 (Linux; U; Android 2.2; en-us; ' +
    'Nexus One Build/FRF91) AppleWebKit/533.1 (KHTML, like Gecko) '+
    'Version/4.0 Mobile Safari/533.1';


var useragentTests = {};

var run = function() {
  var container = document.createElement('div');
  container.id = 'webview-tag-container';
  document.body.appendChild(container);

  chrome.test.getConfig(function(chromeConfig) {
    utils.setUp(chromeConfig, config);
    embedder.loadGuest(function() {
      chrome.test.runTests([
        useragentTests.testUserAgentOverrideNewWindow
      ]);
    }, function(data) {
      return false;
    },
    undefined /* partition */,
    function(webview) {
      chrome.test.assertFalse(webview.isUserAgentOverridden());
      // Called before setting .src attribute.
      webview.setUserAgentOverride(USER_AGENT_OVERRIDE);
    });
  });
};

// Tests.
useragentTests.testUserAgentOverrideNewWindow = function() {
  embedder.webview.addEventListener('newwindow', function(e) {
    window.console.log('Requesting a new window.');
    var webview = embedder.createWebView(embedder.webview.partition);
    webview.setUserAgentOverride(ANDROID_USER_AGENT);
    embedder.setupWebView(webview, function() {
      window.console.log('Created channel with new window.');
      webview.contentWindow.postMessage(
          JSON.stringify(['get-user-agent']), '*');
    }, function(data) {
      if (data[0] != 'got-user-agent')
        return false;

      var userAgent = data[1];
      chrome.test.assertEq(ANDROID_USER_AGENT, userAgent);
      chrome.test.assertEq(ANDROID_USER_AGENT, webview.getUserAgent());
      chrome.test.succeed();
      return true;
    });
    e.window.attach(webview);
  });
  embedder.webview.contentWindow.postMessage(
      JSON.stringify(['open-window', 'about:blank']), '*');
};

// Run test(s).
run();
