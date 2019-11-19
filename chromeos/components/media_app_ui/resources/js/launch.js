// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Helper to call postMessage on the guest frame.
 *
 * @param {Object} message
 */
function postToGuestWindow(message) {
  const guest =
      /** @type{HTMLIFrameElement} */ (document.querySelector('iframe'));
  // The next line should probably be passing a transfer argument, but that
  // causes Chrome to send a "null" message. The transfer seems to work without
  // the third argument (but inefficiently, perhaps).
  guest.contentWindow.postMessage(message, 'chrome://media-app-guest');
}

/**
 * Loads a file in the guest.
 *
 * @param {Blob} blob
 */
async function loadBlob(blob) {
  // It would be better to pass a blob URL here, but that needs cross-origin
  // access to Blob URLs, which should come with https://crbug.com/1012150.
  // Until then, use go/mdn/API/Blob/arrayBuffer (working draft).
  const buffer = await blob.arrayBuffer();
  postToGuestWindow({'buffer': buffer, 'type': blob.type});
}
