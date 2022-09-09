// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.getConfig(function(config) {
  var IN_APP_URL = 'nav-target.html';
  var REMOTE_URL = 'http://localhost:' + config.testServer.port
      '/extensions/platform_apps/navigation/nav-target.html';

  var testForm = document.getElementById('test-form');
  var testLink = document.getElementById('test-link');

  function clickTestLink() {
    var clickEvent = document.createEvent('MouseEvents');
    clickEvent.initMouseEvent('click', true, true, window,
                              0, 0, 0, 0, 0, false, false,
                              false, false, 0, null);

    return testLink.dispatchEvent(clickEvent);
  }

  var tests = [
    // Location functions to in-app URLs.
    function() { window.location = IN_APP_URL },
    function() { window.location.href = IN_APP_URL; },
    function() { window.location.replace(IN_APP_URL); },
    function() { window.location.assign(IN_APP_URL); },

    // Location function to remote URLs (not testing all ways of navigating to
    // it, since that was covered by the previous set)
    function() { window.location = REMOTE_URL; },

    // Form submission (GET, POST, in-app, and remote)
    function() {
      testForm.method = 'GET';
      testForm.action = IN_APP_URL;
      testForm.submit();
    },
    function() {
      testForm.method = 'POST';
      testForm.action = IN_APP_URL;
      testForm.submit();
    },
    function() {
      testForm.method = 'GET';
      testForm.action = REMOTE_URL;
      testForm.submit();
    },
    function() {
      testForm.method = 'POST';
      testForm.action = REMOTE_URL;
      testForm.submit();
    },

    // Clicks on links (in-app and remote).
    function() { testLink.href = IN_APP_URL; clickTestLink(); },
    function() { testLink.href = REMOTE_URL; clickTestLink(); },

    // Link with target blank and a remote URL opens a new tab in the browser
    // (verified in C++).
    function() {
      testLink.target = '_blank';
      testLink.href = 'http://chromium.org';
      clickTestLink();
    },
    // If we manage to execute this test case, then we haven't navigated away.
    function() { chrome.test.notifyPass(); }
  ];

  var testIndex = 0;

  function runTest() {
    var test = tests[testIndex++];

    console.log('Testing ' + (testIndex - 1) + ': ' + test);
    test();

    if (testIndex < tests.length) {
      // Don't run the next test immediately, since navigation happens
      // asynchronously, and if we do end up navigating, we don't want to still
      // execute the final test case (which signals success).
      window.setTimeout(runTest, 100);
    }
  }

  runTest();
});
