// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// META script=resources/controlled_frame_helpers.js

// Verifies that the callback form of API calls are not allowed. This is
// necessary since the WebView API natively supports callbacks while Controlled
// Frame adapts that interface to be promises-based.  Specific Controlled Frame
// changes are in place to restrict those asynchronous calls so the callback is
// not available.

const kCallbackErr = "Callback form deprecated, see API doc for correct usage.";

function expectCallbackDeprecationError(f) {
  return new Promise((resolve, reject) => {
    try {
      f(() => reject('FAIL: Expected the callback to not be called.'));
      reject('FAIL: Call did not throw an error as expected.');
      return;
    } catch (e) {
      const actualErrorMessage = e.message;
      if (actualErrorMessage !== kCallbackErr) {
        reject('FAIL: Unexpected error: ' + actualErrorMessage);
        return;
      }
      resolve('SUCCESS');
      return;
    }
    reject('FAIL: Unexpected error');
  });
}

promise_test(async (test) => {
  const controlledFrame = await createControlledFrame('/simple.html');
  await expectCallbackDeprecationError((callback) => {
    controlledFrame.executeScript(
        {code: "document.body.style.backgroundColor = 'red';"}, callback);
  });
}, "Verify no callbacks are allowed for executeScript");

promise_test(async (test) => {
  const controlledFrame = await createControlledFrame('/simple.html');
  await expectCallbackDeprecationError((callback) => {
    controlledFrame.addContentScripts(
        [{
          name: 'test',
          matches: ['https://*/*'],
          js: {files: ['/resources/content_script.js']},
          run_at: 'document_start',
        }],
        callback);
  });
}, 'Verify no callbacks are allowed for addContentScripts');

promise_test(async (test) => {
  const controlledFrame = await createControlledFrame('/simple.html');
  await expectCallbackDeprecationError((callback) => {
    controlledFrame.contextMenus.remove(/*id=*/"1", callback);
  });
}, "Verify no callbacks are allowed for Context Menus .remove()");

promise_test(async (test) => {
  const controlledFrame = await createControlledFrame('/simple.html');
  await expectCallbackDeprecationError((callback) => {
    controlledFrame.contextMenus.removeAll(callback);
  });
}, "Verify no callbacks are allowed for Context Menus .removeAll()");

promise_test(async (test) => {
  const controlledFrame = await createControlledFrame('/simple.html');
  await expectCallbackDeprecationError((callback) => {
    controlledFrame.contextMenus.update(/*id=*/"1", {title: "Title"}, callback);
  });
}, "Verify no callbacks are allowed for Context Menus .update()");
