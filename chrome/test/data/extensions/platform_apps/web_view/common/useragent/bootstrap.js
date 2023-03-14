// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

config.IS_CHROME_TEST = true;
// No TestServer.
config.IS_JS_ONLY_GUEST = true;
config.TEST_DIR = 'useragent';
var USER_AGENT_OVERRIDE = 'Mozilla/5.0 (X11; U; Linux x86_64; en-US) ' +
    'AppleWebKit/540.0 (KHTML,like Gecko) Chrome/9.1.0.0 Safari/540.0';

var useragentTests = {};

var run = function() {
  var container = document.createElement('div');
  container.id = 'webview-tag-container';
  document.body.appendChild(container);

  var step = 1;

  chrome.test.getConfig(function(chromeConfig) {
    utils.setUp(chromeConfig, config);
    embedder.loadGuest(function() {
      chrome.test.runTests([
        useragentTests.testUserAgentOverride
      ]);
    }, function(data) {
      if (data[0] == 'got-user-agent') {
        // data[1] is the guest's user agent.
        if (step == 1) {
          chrome.test.assertEq(USER_AGENT_OVERRIDE, data[1]);
          chrome.test.assertEq(USER_AGENT_OVERRIDE,
                               embedder.webview.getUserAgent());
          embedder.webview.setUserAgentOverride('foobar');
        } else if (step == 2) {
          chrome.test.assertEq('foobar', data[1]);
          chrome.test.assertTrue(embedder.webview.isUserAgentOverridden());
          chrome.test.assertEq('foobar', embedder.webview.getUserAgent());

          // Now remove the UA override.
          embedder.webview.setUserAgentOverride('');
        } else if (step == 3) {
          chrome.test.assertNe(data[1], 'foobar');
          chrome.test.assertFalse(embedder.webview.isUserAgentOverridden());
          chrome.test.succeed();
        }

        step++;
        return true;
      }
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
useragentTests.testUserAgentOverride = function() {
  embedder.webview.contentWindow.postMessage(
      JSON.stringify(['get-user-agent']), '*');
};

// Run test(s).
run();
