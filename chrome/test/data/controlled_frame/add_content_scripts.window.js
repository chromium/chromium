// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// META script=resources/controlled_frame_helpers.js

function reload(controlledFrame) {
  return new Promise((resolve, reject) => {
    const onAbort = () => {
      controlledFrame.removeEventListener('loadabort', onAbort);
      controlledFrame.removeEventListener('loadcommit', onCommit);
      reject();
    };
    const onCommit = () => {
      controlledFrame.removeEventListener('loadabort', onAbort);
      controlledFrame.removeEventListener('loadcommit', onCommit);
      resolve();
    };
    controlledFrame.addEventListener('loadabort', onAbort);
    controlledFrame.addEventListener('loadcommit', onCommit);

    // HTMLControlledFrameElement.reload() doesn't actually trigger the
    // loadcommit or loadabort events. Appending a new query parameter to the
    // URL will force a new navigation.
    controlledFrame.src += '&reload';
  });
}

async function getBackgroundColor(controlledFrame) {
  return await executeAsyncScript(
      controlledFrame, 'document.body.style.backgroundColor');
}

promise_test(async (test) => {
  const controlledFrame = await createControlledFrame('/simple.html?');

  await controlledFrame.addContentScripts([{
    name: 'test',
    urlPatterns: ['https://*/*'],
    js: {files: ['/resources/content_script.js']},
    runAt: 'document-start',
  }]);

  assert_equals('', await getBackgroundColor(controlledFrame));

  await reload(controlledFrame);

  assert_equals('red', await getBackgroundColor(controlledFrame));

  await controlledFrame.removeContentScripts(['test']);
  await reload(controlledFrame);

  assert_equals('', await getBackgroundColor(controlledFrame));
}, 'Content scripts run');

promise_test(async (test) => {
  const controlledFrame = await createControlledFrame('/simple.html?');

  const result = controlledFrame.addContentScripts([{
    name: 'test',
    urlPatterns: ['*invalid pattern*'],
    js: {files: ['/resources/content_script.js']},
    runAt: 'document-start',
  }]);

  return promise_rejects_js(test, TypeError, result, 'Promise should reject');
}, 'Reject a content script with invalid pattern');
