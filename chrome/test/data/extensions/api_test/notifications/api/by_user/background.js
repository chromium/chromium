// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const notifications = chrome.notifications;
var theOnlyTestDone = null;

var notificationData = {
  type: "basic",
  iconUrl: "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAA" +
           "CNbyblAAAAHElEQVQI12P4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHw" +
           "AAAABJRU5ErkJggg==",
  title: "Attention!",
  message: "Check out Cirque du Soleil"
};

var results = {
  FOO: false,
  BAR: true,
  BAT: false,
  BIFF: false,
  BLAT: true,
  BLOT: true
};

function createCallback(id) { }

function notifyPass() { chrome.test.notifyPass(); }

var onClosedHooks = {
  FOO: notifyPass,
  BAR: notifyPass,
  BIFF: function() {
    notifications.create("BLAT", notificationData, function () {
      if (chrome.runtime.lastError) {
        chrome.test.notifyFail(lastError.message);
        return;
      }
      notifications.create("BLOT", notificationData, function () {
        if (chrome.runtime.lastError) {
          chrome.test.notifyFail(lastError.message);
          return;
        }
        chrome.test.notifyPass("Created the new notifications.");
      });
    });
  },
};

function onClosedListener(id, by_user) {
  if (results[id] !== by_user) {
    chrome.test.notifyFail("Notification " + id +
                           " closed with bad by_user param ( "+ by_user +" )");
    return;
  }
  delete results[id];

  if (typeof onClosedHooks[id] === "function")
    onClosedHooks[id]();

  if (Object.keys(results).length === 0) {
    chrome.test.notifyPass("Done!");
    theOnlyTestDone();
  }
}

notifications.onClosed.addListener(onClosedListener);

function theOnlyTest() {
  // This test coordinates with the browser test.  First, 4 notifications are
  // created.  Then 2 are manually cancelled in C++.  Then clearAll is called
  // with false |by_user|.  Then once the BIFF notification is cleared, we
  // create two more notifications in JS, and C++ calls the clearAll with true
  // |by_user|.
  theOnlyTestDone = chrome.test.callbackAdded();

  notifications.create("FOO", notificationData, createCallback);
  notifications.create("BAR", notificationData, createCallback);
  notifications.create("BAT", notificationData, createCallback);
  notifications.create("BIFF", notificationData, createCallback);
}

chrome.test.runTests([ theOnlyTest ]);
