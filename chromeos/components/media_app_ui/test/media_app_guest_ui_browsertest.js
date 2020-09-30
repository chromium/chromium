// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for chrome-untrusted://media-app. */

// Test web workers can be spawned from chrome-untrusted://media-app. Errors
// will be logged in console from web_ui_browser_test.cc.
GUEST_TEST('GuestCanSpawnWorkers', async () => {
  let caughtError = null;
  let networkErrorEventType = '';

  try {
    // The "real" webworker isn't needed for the mock, but we want to test CSP
    // errors. Fetch something that doesn't exist. This has a bonus that we can
    // wait for an "error" event, whereas fetching a "real" file would require
    // it to respond to a postMessage. Note the resource will 404, but that
    // won't throw an exception. CSP errors, however, will throw errors that
    // fail the test. E.g., Failed to construct 'Worker': Script at
    // 'about:blank' cannot be accessed from origin
    // 'chrome-untrusted://media-app'.
    const worker = new Worker('/non-existent.js');

    // Await the network error to ensure things get that far. The error itself
    // is entirely opaque.
    networkErrorEventType = await new Promise(resolve => {
      worker.onerror = (event) => {
        resolve(event.type);
      };
    });
  } catch (e) {
    caughtError = e;
  }

  assertEquals(caughtError, null, caughtError && caughtError.message);
  assertEquals(networkErrorEventType, 'error');
});

// Test that language is set correctly on the guest iframe.
GUEST_TEST('GuestHasLang', () => {
  assertEquals(document.documentElement.lang, 'en-US');
});

// Test can load files with CSP restrictions. We expect `error` to be called
// as these tests are loading resources that don't exist. Note: we can't violate
// CSP in tests or Js Errors will cause test failures.
GUEST_TEST('GuestCanLoadWithCspRestrictions', async () => {
  // Can load images served from chrome-untrusted://media-app/.
  const image = new Image();
  image.src = 'chrome-untrusted://media-app/does-not-exist.png';
  await test_util.eventToPromise('error', image);

  // Can load image data urls.
  const imageData = new Image();
  imageData.src = 'data:image/png;base64,iVBORw0KG';
  await test_util.eventToPromise('error', imageData);

  // Can load image blobs.
  const imageBlob = new Image();
  imageBlob.src = 'blob:chrome-untrusted://media-app/my-fake-blob-hash';
  await test_util.eventToPromise('error', imageBlob);

  // Can load video blobs.
  const videoBlob =
      /** @type {!HTMLVideoElement} */ (document.createElement('video'));
  videoBlob.src = 'blob:chrome-untrusted://media-app/my-fake-blob-hash';
  await test_util.eventToPromise('error', videoBlob);
});
