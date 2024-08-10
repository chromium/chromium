// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([function registerListeners() {
  // Add the maximum number of listeners.
  var listeners = [];
  var MAX_LISTENERS = 10;
  for (var i = 0; i < MAX_LISTENERS; i++) {
    listeners.push({
      'callback': function() {},
      'filter': {'serviceType': '_' + i + '._tcp.local'}
    });
  }
  listeners.forEach(function(x) {
    chrome.mdns.onServiceList.addListener(x.callback, x.filter);
  });

  var removeAllListeners = function() {};

  // Now try adding one more.  This should exceed the limit.
  var failedToAddLast = false;
  try {
    chrome.mdns.onServiceList.addListener(function() {}, {
      'serviceType': '_one_too_many._tcp.local'
    });
  } catch (e) {
    chrome.test.assertTrue(
        e.message.indexOf('Too many listeners') != -1, e.message);
    failedToAddLast = true;
  }
  chrome.test.assertTrue(
      failedToAddLast,
      'there should be an error when adding the 11th listener');

  listeners.forEach(function(x) {
    chrome.mdns.onServiceList.removeListener(x.callback, x.filter);
  });
  chrome.test.notifyPass();
}]);
