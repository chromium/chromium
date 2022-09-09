// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var current = chrome.app.window.current();
var bg = null;
var nextTestNumber = 1;

function makeEventTest(eventName, startFunction) {
  var test = function() {
    bg.clearEventCounts();
    var listener = function() {
      current[eventName].removeListener(listener);
      function waitForBackgroundPageToSeeEvent() {
        if (!bg.eventCounts[eventName] > 0) {
          bg.eventCallback = waitForBackgroundPageToSeeEvent;
        }
        else {
          bg.eventCallback = null;
          current.restore();
          chrome.test.succeed();
        }
      }
      waitForBackgroundPageToSeeEvent();
    };
    current[eventName].addListener(listener);
    startFunction();
  };
  // For anonymous functions, setting 'generatedName' controls what shows up in
  // the apitest framework's logging output.
  test.generatedName = "Test" + nextTestNumber++  + "_" + eventName;
  return test;
}


var tests = {
  minimized: [
    makeEventTest(
        'onMinimized',
        function() {
          current.minimize();
        }),
  ],
  maximized: [
    makeEventTest(
        'onMaximized',
        function() {
          current.maximize();
        }),
  ],
  restored: [
    makeEventTest(
        'onRestored',
        function() {
          var doRestore = function() {
            current.onMinimized.removeListener(doRestore);
            current.restore();
          };
          current.onMinimized.addListener(doRestore);
          current.minimize();
        }),
    makeEventTest(
        'onRestored',
        function() {
          var doRestore = function() {
            current.onMaximized.removeListener(doRestore);
            current.restore();
          };
          current.onMaximized.addListener(doRestore);
          current.maximize();
        })
  ],
  boundsChanged: [
    makeEventTest(
        'onBoundsChanged',
        function() {
          current.outerBounds.setPosition(5, 5);
        }),
    makeEventTest(
        'onBoundsChanged',
        function() {
          current.outerBounds.setSize(150, 150);
        }),
    makeEventTest(
        'onBoundsChanged',
        function() {
          current.innerBounds.setPosition(40, 40);
        }),
    makeEventTest(
        'onBoundsChanged',
        function() {
          current.innerBounds.setSize(100, 100);
        })
  ],
};

chrome.runtime.getBackgroundPage(function(page) {
  bg = page;
  chrome.test.getConfig(function(config) {
    if (config.customArg in tests)
      chrome.test.runTests(tests[config.customArg]);
    else
      chrome.test.fail('Test "' + config.customArg + '"" doesnt exist!');
  });
});
