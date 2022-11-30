// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var testTabId_;

console.log("audible start");

function getOnlyTab() {
  var views = chrome.extension.getViews({type: "tab"});
  assertEq(1, views.length);
  return views[0];
}

chrome.test.runTests([
  function setupWindow() {
    console.log("setupwindow");

    chrome.tabs.getCurrent(pass(function(tab) {
      testTabId_ = tab.id;
    }));
  },

  function audibleStartsFalse() {
    console.log("audiblestartsfirst");

    chrome.tabs.get(testTabId_, pass(function(tab) {
      assertEq(false, tab.audible);
      queryForTab(testTabId_, {audible: false}, pass(function(tab) {
        assertEq(false, tab.audible);
      }));
      queryForTab(testTabId_, {audible: true}, pass(function(tab) {
        assertEq(null, tab);
      }));
    }));
  },

  function audibleUpdateAttemptShouldFail() {
    var expectedError =
        'Error in invocation of tabs.update(' +
        'optional integer tabId, object updateProperties, ' +
        'optional function callback): Error at parameter ' +
        '\'updateProperties\': Unexpected property: \'audible\'.';

    try {
      chrome.tabs.update(testTabId_, {audible: true}, function(tab) {
        chrome.test.fail('Updated audible property via chrome.tabs.update');
      });
    } catch (e) {
      assertEq(expectedError, e.message);
      chrome.test.succeed();
    }
  },

  function makeAudible() {
    onUpdatedExpect("audible", true, null);
    window.sinewave.play(getOnlyTab(), 200);
  },

  function testStaysAudibleAfterChangingWindow() {
    chrome.windows.create({}, pass(function(window)
    {
      chrome.tabs.move(testTabId_, {windowId: window.id, index: -1},
                       pass(function(tab) {
        assertEq(true, tab.audible);
      }));
    }));
  },

  function makeNotAudible() {
    onUpdatedExpect("audible", false, null);
    window.sinewave.stop(getOnlyTab());
  }
]);
