// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// META script=resources/controlled_frame_helpers.js

const makeBadSchemeCheck = function(test, badURL) {
  return new Promise((resolve, reject) => {
    try {
      const frame = document.createElement('controlledframe');
      let loadAbortCalled = false;
      frame.addEventListener('loadabort', function(e) {
        try {
          assert_equals(e.reason, 'ERR_DISALLOWED_URL_SCHEME');
          assert_equals(e.url, badURL);
          // The test does not complete here. After the loadabort event, the
          // frame should redirect to about:blank and send a loadstop event.
          loadAbortCalled = true;
        } catch (e) {
          reject(e);
        }
      });
      frame.addEventListener('loadstop', function(e) {
        try {
          assert_equals(loadAbortCalled, true);
          assert_equals(frame.src, 'about:blank');
          resolve();
        } catch (e) {
          reject(e);
        }
      });
      frame.addEventListener('exit', function() {
        reject("Exit listener called. This indicates that the " +
               "Controlled Frame's content crashed which should not occur.");
      });
      frame.src = badURL;
      document.body.appendChild(frame);
    } catch (e) {
      reject(e);
    }
  });
};

// Verifies that navigation to a URL that is valid but not web-safe or
// pseudo-scheme fires loadabort and doesn't cause a crash.

promise_test(async (test) => {
  return makeBadSchemeCheck(test, 'chrome://abc123/');
}, "Verify bad scheme chrome:// aborts, does not load");

promise_test(async (test) => {
  return makeBadSchemeCheck(test, 'chrome-extension://abc123/');
}, "Verify bad scheme chrome-extension: aborts, does not load");

promise_test(async (test) => {
  return makeBadSchemeCheck(test, 'chrome-untrusted://abc123/');
}, "Verify bad scheme chrome-untrusted: aborts, does not load");

promise_test(async (test) => {
  return makeBadSchemeCheck(test, 'filesystem:http://abc123/temp.png/');
}, "Verify bad scheme filesystem: aborts, does not load");

promise_test(async (test) => {
  return makeBadSchemeCheck(test, 'javascript://abc123/');
}, "Verify bad scheme javascript: aborts, does not load");

promise_test(async (test) => {
  return makeBadSchemeCheck(test, 'ws://abc123/');
}, "Verify bad scheme ws: aborts, does not load");

promise_test(async (test) => {
  return makeBadSchemeCheck(test, 'wss://abc123/');
}, "Verify bad scheme wss: aborts, does not load");
