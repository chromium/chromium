// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.app.runtime.onLaunched.addListener(function() {
  var IN_APP_RELATIVE_URL = 'nav-target.html';
  var IN_APP_ABSOLUTE_URL = chrome.runtime.getURL(IN_APP_RELATIVE_URL);
  var WEB_URL = 'http://chromium.org';

  var testLink = document.createElement('a');
  document.body.appendChild(testLink);

  function clickTestLink() {
    var clickEvent = document.createEvent('MouseEvents');
    clickEvent.initMouseEvent('click', true, true, window,
                              0, 0, 0, 0, 0, false, false,
                              false, false, 0, null);

    return testLink.dispatchEvent(clickEvent);
  }

  // Tests are verified in C++ at the end of the run, by looking at the tabs
  // these tests create.
  var tests = [
    // Trying to open a local resource in a new window will fail.
    function windowOpenInAppRelativeURL() {
      var w = window.open(IN_APP_RELATIVE_URL);
      chrome.test.assertTrue(!w);
      chrome.test.succeed();
    },
    // Trying to open a local resource in a new window will fail.
    function openLinkToInAppRelativeURL() {
      testLink.target = '_blank';
      testLink.href = IN_APP_RELATIVE_URL;
      clickTestLink();
      chrome.test.succeed();
    },
    // Similar to windowOpenInAppRelativeURL().
    function windowOpenInAppAbsoluteURL() {
      var w = window.open(IN_APP_ABSOLUTE_URL);
      chrome.test.assertTrue(!w);
      chrome.test.succeed();
    },
    // Similar to openLinkToInAppRelativeURL().
    function openLinkToInAppAbsoluteURL() {
      testLink.target = '_blank';
      testLink.href = IN_APP_ABSOLUTE_URL;
      clickTestLink();
      chrome.test.succeed();
    },
    // Opening web links in new window will pass.
    // The opened tab will be verified in C++.
    function windowOpenWebURL() {
      var w = window.open(WEB_URL);
      chrome.test.assertTrue(!!w);
      chrome.test.succeed();
    },
    // Opening web links in new window will pass.
    // The opened tab will be verified in C++.
    function openLinkToWebURL() {
      testLink.target = '_blank';
      testLink.href = WEB_URL;
      clickTestLink();
      chrome.test.succeed();
    }
  ];
  chrome.test.runTests(tests);
});
