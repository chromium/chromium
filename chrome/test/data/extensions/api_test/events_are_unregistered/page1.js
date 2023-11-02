// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

// Register for events in 4 configurations, then navigate to page2.html, which
// will notify success and succeed the test on the C++ side. The C++ code
// asserts that the events have been unregistered.

//
// Unfiltered events.
//

// A single listener.
chrome.browserAction.onClicked.addListener(function() {});
// Multiple listeners for the same event.
chrome.runtime.onStartup.addListener(function() {});
chrome.runtime.onStartup.addListener(function() {});
// A single listener, which previously had multiple listeners.
{
  let singleListener = function() {};
  chrome.runtime.onSuspend.addListener(singleListener);
  chrome.runtime.onSuspend.addListener(function() {});
  chrome.runtime.onSuspend.removeListener(singleListener);
}
// No listeners, which previously had listeners (all were removed).
{
  let listener1 = function() {};
  let listener2 = function() {};
  chrome.runtime.onInstalled.addListener(listener1);
  chrome.runtime.onInstalled.addListener(listener2);
  chrome.runtime.onInstalled.removeListener(listener1);
  chrome.runtime.onInstalled.removeListener(listener2);
}

//
// Filtered events.
//

function filterPort(portNumber) {
  return {url: [{ports: [portNumber]}]};
}

// A single listener.
chrome.webNavigation.onBeforeNavigate.addListener(function() {});
// Multiple, different listeners.
chrome.webNavigation.onCommitted.addListener(function() {});
chrome.webNavigation.onCommitted.addListener(function() {});
// Different listeners with the same filter.
chrome.webNavigation.onDOMContentLoaded.addListener(
    function() {}, filterPort(80));
chrome.webNavigation.onDOMContentLoaded.addListener(
    function() {}, filterPort(80));
// Different listeners with different filters, same event added twice.
{
  let singleListener = function() {};
  chrome.webNavigation.onCompleted.addListener(
      function() {}, filterPort(80));
  chrome.webNavigation.onCompleted.addListener(
      function() {}, filterPort(81));
  chrome.webNavigation.onCompleted.addListener(
      function() {}, filterPort(81));
  chrome.webNavigation.onCompleted.addListener(
      singleListener, filterPort(82));
  chrome.webNavigation.onCompleted.removeListener(singleListener);
}

location.assign('page2.html');
