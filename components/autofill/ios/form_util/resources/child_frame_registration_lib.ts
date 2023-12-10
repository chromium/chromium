// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Identifies relationships between parent and child frames
 * by generating a unique ID and sending it to the browser from each frame.
 */

import {generateRandomId} from '//ios/web/public/js_messaging/resources/frame_id.js';

/**
 * Generates a new remote ID for `frame`, and posts it to `frame`, so that
 * `frame` can register itself with the browser layer as the frame corresponding
 * to the new remote ID.
 * @param {HTMLIFrameElement} frame The frame to be registered.
 * @return {string} The newly-generated remote ID associated with `frame`.
 *     Because registration happens asynchronously over message passing, it
 *     should not be assumed that this frame ID will be known to the browser by
 *     the time this function completes.
 */
function registerChildFrame(_frame: HTMLIFrameElement): string {
  const remoteFrameId: string = generateRandomId();

  // TODO(crbug.com/1440471): Pass `remoteFrameId` to `frame` and have
  // `frame` register itself with the browser.

  return remoteFrameId;
}

export {registerChildFrame};
