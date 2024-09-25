// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// META script=resources/controlled_frame_helpers.js

// Verifies that for executeScript, the callback form of the call is
// not allowed. This is necessary since the WebView API natively supports
// callbacks while Controlled Frame adapts that interface to be promises-based.
// Specific Controlled Frame changes are in place to restrict those asynchronous
// calls so the callback is not available.

const kCallbackErr = "Callback form deprecated, see API doc for correct usage.";

promise_test((test) => {
  return new Promise((resolve, reject) => {
    const frame = document.createElement('controlledframe');
    if (!frame || !frame.request) {
      reject('FAIL');
      return;
    }

    function handleExecute() {
      let actual_error_message;
      try {
        frame.executeScript(
          {code: "document.body.style.backgroundColor = 'red';"},
          () => { reject('FAIL: Expected the callback to not be called.'); });
        reject('FAIL: Call did not throw an error as expected.');
        return;
      } catch (e) {
        actual_error_message = e.message;
        if (actual_error_message !== kCallbackErr) {
          reject('FAIL: Unexpected error: ' + actual_error_message);
          return;
        }
        resolve('SUCCESS');
        return;
      }
      reject('FAIL: Unexpected error');
    }

    frame.src = 'data:text/html,<body>Guest</body>';
    frame.addEventListener('loadabort', reject);
    frame.addEventListener('loadstop', handleExecute);
    document.body.appendChild(frame);
  });
}, "Verify no callbacks are allowed for executeScript");
