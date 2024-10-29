// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test access into the Child Frame Registration lib.
 * Requires functions in child_frame_registration_lib.ts.
 */

import {gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';

/**
 * Calls registerChildFrame on each frame in the document. This is a convenience
 * method for testing from the C++ layer.
 * @return {string[]} The list of remote IDs sent to the child frames.
 */
function registerAllChildFrames(): string[] {
  const ids: string[] = [];
  for (const frame of document.getElementsByTagName('iframe')) {
    ids.push(gCrWeb.remoteFrameRegistration.registerChildFrame(
        (frame as HTMLIFrameElement)));
  }
  return ids;
}

window.addEventListener(
    'message', gCrWeb.remoteFrameRegistration.processChildFrameMessage);

gCrWeb.remoteFrameRegistration.registerAllChildFrames = registerAllChildFrames;
