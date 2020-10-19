// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Set when PiexLoader has an unrecoverable error to disable future attempts.
 * @type {boolean}
 */
let piexEnabled = true;

/** Handles wasm load failures. */
function onPiexModuleFailed() {
  piexEnabled = false;
}

/**
 * Extracts a JPEG from a RAW Image ArrayBuffer.
 * @param {!ArrayBuffer} buffer
 * @return {!Promise<!File>}
 */
async function extractFromRawImageBuffer(buffer) {
  if (!piexEnabled) {
    throw new Error('Piex disabled');
  }
  const response = await PiexLoader.load(buffer, onPiexModuleFailed);
  // Note the "thumbnail" is usually the full-sized image "preview", but may
  // fall back to a thumbnail when that is unavailable.
  // The mime type may be unsupported - let the caller deal with that.
  // TOD(b/169717921): Apply `response.orientation`.
  return new File(
      [response.thumbnail], 'raw-preview', {type: response.mimeType});
}
