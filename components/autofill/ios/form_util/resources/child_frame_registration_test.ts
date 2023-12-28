// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test access into the Child Frame Registration lib.
 */

import {processChildFrameMessage, registerChildFrame} from '//components/autofill/ios/form_util/resources/child_frame_registration_lib.js';
import {gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';

/**
 * Calls registerChildFrame on each frame in the document. This is a convenience
 * method for testing from the C++ layer.
 * @return {string[]} The list of remote IDs sent to the child frames.
 */
function registerAllChildFrames(): string[] {
  const ids: string[] = [];
  for (const frame of document.getElementsByTagName('iframe')) {
    ids.push(registerChildFrame((frame as HTMLIFrameElement)));
  }
  return ids;
}

window.addEventListener('message', processChildFrameMessage);

gCrWeb.childFrameRegistrationTesting = {
  registerChildFrame,
  registerAllChildFrames,
};
