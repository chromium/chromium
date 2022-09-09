// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var allTests = [
  function testFocusAcrossWindows() {
    // Create two windows with focusable text fields in them.
    // Let window2 have focus. (Enforce this by focusing the WebView
    // that's the parent of the second window's document, below.)
    var html1 = '<input title="Input1">';
    var html2 = '<input title="Input2">';
    var url1 = 'data:text/html,<!doctype html>' + encodeURI(html1);
    var url2 = 'data:text/html,<!doctype html>' + encodeURI(html2);
    chrome.windows.create({url: url1, focused: false});
    chrome.windows.create({url: url2, focused: true});

    // Wait for the accessibility trees to load and get the accessibility
    // objects for both text fields.
    //
    // Try to focus the first text field (in the unfocused window) and then
    // the second text field (in the focused window) after a short delay.
    //
    // If we get a focus event on the first text field, the test fails,
    // because that window is in the background. If we get a focus event on
    // the second text field, the test succeeds.
    var input1, input2;
    chrome.automation.getDesktop(function(rootNode) {
      rootNode.addEventListener('loadComplete', function(event) {
        if (event.target.url == url1) {
          input1 = event.target.find({role: 'textField'});
        }
        if (event.target.url == url2) {
          // Focus the WebView that's the parent of the second document.
          event.target.parent.focus();
          input2 = event.target.find({role: 'textField'});
        }
        if (input1 && input2) {
          input1.addEventListener('focus', function(event) {
            chrome.test.fail();
          }, false);
          input2.addEventListener('focus', function(event) {
            chrome.test.succeed();
          }, false);
          input1.focus();
          setTimeout(function() {
            input2.focus();
          }, 100);
        }
      }, false);
    });
  },
];

chrome.test.runTests(allTests);
