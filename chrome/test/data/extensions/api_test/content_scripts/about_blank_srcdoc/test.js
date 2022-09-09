// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.getConfig(function(config) {
  chrome.test.runTests([
    function testAboutBlankInFrame() {
      chrome.runtime.onMessage.addListener(function onMessageListener(message) {
        chrome.runtime.onMessage.removeListener(onMessageListener);
        chrome.test.assertEq('message from about:blank', message);
        chrome.test.succeed();
      });
      chrome.test.log('Creating tab...');
      var test_url =
          ('http://localhost:PORT/extensions/' +
           'test_file_with_about_blank_iframe.html')
              .replace(/PORT/, config.testServer.port);
      chrome.tabs.create({ url: test_url });
    },
    function testAboutSrcdocFrame() {
      chrome.runtime.onMessage.addListener(function onMessageListener(message) {
        chrome.runtime.onMessage.removeListener(onMessageListener);
        chrome.test.assertEq('message from about:srcdoc', message);
        chrome.test.succeed();
      });
      chrome.test.log('Creating tab...');
      var test_url =
          ('http://localhost:PORT/extensions/' +
           'api_test/webnavigation/srcdoc/a.html')
              .replace(/PORT/, config.testServer.port);
      chrome.tabs.create({ url: test_url });
    },
    // Tests that content scripts are inserted in about:blank frames, even if
    // they are embedded in another about:-frame.
    function testAboutSrcdocNestedFrame() {
      chrome.runtime.onMessage.addListener(function onMessageListener(message) {
        chrome.runtime.onMessage.removeListener(onMessageListener);
        // Child frame of srcdoc document
        chrome.test.assertEq('message from about:blank', message);
        chrome.runtime.onMessage.addListener(function secondListener(message) {
          chrome.runtime.onMessage.removeListener(secondListener);
          // Child frame of top-level document
          chrome.test.assertEq('message from about:srcdoc', message);
          chrome.test.succeed();
        });
      });
      chrome.test.log('Creating tab...');
      var test_url =
          ('http://localhost:PORT/extensions/' +
           'test_file_with_about_blank_in_srcdoc.html')
              .replace(/PORT/, config.testServer.port);
      chrome.tabs.create({ url: test_url });
    },
    function testAboutBlankInTopLevelFrame() {
      chrome.runtime.onMessage.addListener(function onMessageListener(message) {
        chrome.runtime.onMessage.removeListener(onMessageListener);
        chrome.test.assertEq('message from about:blank', message);
        chrome.test.succeed();
      });

      // Create tab using window.open on an existing page to inherit the origin
      // of the document.
      chrome.test.log('Creating tab using window.open("about:blank")...');
      chrome.tabs.query({
        url: '*://*/*test_file_with_about_blank_iframe*'
      }, function(tabs) {
        chrome.test.assertTrue(tabs.length > 0);
        chrome.tabs.sendMessage(tabs[0].id, 'open about:blank popup');
      });
    },

    // Request host permissions for chrome.tabs.executeScript. These permissions
    // were declared as optional, to make sure that the permission check for
    // about:blank is tested against the content_scripts[*].matches in the
    // manifest file instead of the effective permissions.
    function getHostPermissionsForFollowingTests() {
      chrome.permissions.request({
        origins: ['*://*/*']
      }, function(granted) {
        chrome.test.assertTrue(granted,
            'Need *://*/* permissions for chrome.tabs.executeScript tests.');
        chrome.test.succeed();
      });
    },

    // chrome.tabs.executeScript should run in the frame
    function testExecuteScriptInFrame() {
      chrome.tabs.query({
        url: '*://*/*test_file_with_about_blank_iframe*'
      }, function(tabs) {
        chrome.test.assertTrue(tabs.length > 0);
        chrome.tabs.executeScript(tabs[0].id, {
          code: 'frameElement && (frameElement.src || "about:blank")',
          matchAboutBlank: true,
          allFrames: true
        }, function(results) {
          chrome.test.assertNoLastError();
          chrome.test.assertTrue(results.indexOf('about:blank') >= 0);
          chrome.test.succeed();
        });
      });
    },
    // chrome.tabs.executeScript should run in about:srcdoc frames
    function testExecuteScriptInSrcdocFrame() {
      chrome.tabs.query({
        url: '*://*/*srcdoc*'
      }, function(tabs) {
        chrome.test.assertTrue(tabs.length > 0);
        chrome.tabs.executeScript(tabs[0].id, {
          code: 'frameElement && frameElement.srcdoc && "srcdoc"',
          matchAboutBlank: true,
          allFrames: true
        }, function(results) {
          chrome.test.assertNoLastError();
          chrome.test.assertTrue(results.indexOf('srcdoc') >= 0,
              'contentscript should be able to run in srcdoc frame');
          chrome.test.succeed();
        });
      });
    }
  ]);
});
