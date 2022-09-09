// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const notifications = chrome.notifications;

var idString = "foo";

var testBasicEvents = function() {
  var incidents = 0;

  var onCreateCallback = function(id) {
    chrome.test.assertTrue(id.length > 0);
    chrome.test.assertEq(idString, id);
    chrome.test.succeed();
  }

  var options = {
    type: "basic",
    iconUrl: "icon.png",
    title: "Attention!",
    message: "Check out Cirque du Soleil"
  };
  notifications.create(idString, options, onCreateCallback);
};

chrome.test.runTests([ testBasicEvents ]);
