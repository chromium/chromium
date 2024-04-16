// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Identifies relationships between parent and child frames
 * by generating a unique ID and sending it to the browser from each frame.
 */

import {generateRandomId, getFrameId} from '//ios/web/public/js_messaging/resources/frame_id.js';
import {gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';
import {sendWebKitMessage} from '//ios/web/public/js_messaging/resources/utils.js';

/**
 * The name of the message handler in C++ land which will process registration
 * messages. This corresponds to FormHandlersJavaScriptFeature; if this lib
 * is reused in non-autofill contexts, this hardcoded value should be replaced
 * with a param.
 */
const NATIVE_MESSAGE_HANDLER = 'FormHandlersMessage';

/**
 * An identifying string used by interframe messages.
 */
const REGISTER_AS_CHILD_FRAME_COMMAND = 'registerAsChildFrame';

/**
 * How long to wait before first posting a message to the child frame, to
 * improve the chance that it actually loads before the message is sent.
 */
const INTERFRAME_MESSAGE_DELAY_MS = 100;

/**
 * Registers the local/remote ID pair with the C++ layer.
 * @param {string} remoteId The ID to be used as the remote frame token.
 */
function registerSelfWithRemoteToken(remoteId: string): void {
  sendWebKitMessage(NATIVE_MESSAGE_HANDLER, {
    'command': REGISTER_AS_CHILD_FRAME_COMMAND,
    'local_frame_id': getFrameId(),
    'remote_frame_id': remoteId,
  });
}

/**
 * Event handler for messages received via window.postMessage.
 * @param {MessageEvent} payload The data sent via postMessage.
 */
function processChildFrameMessage(payload: MessageEvent): void {
  if (!gCrWeb.autofill_form_features.isAutofillAcrossIframesEnabled()) {
    return;
  }
  const command: string|undefined = payload.data?.command;
  if (command && command == REGISTER_AS_CHILD_FRAME_COMMAND) {
    // TODO(crbug.com/1440471): this should send an Ack (see below).
    const remoteId = payload.data?.remoteFrameId;
    if (typeof remoteId == 'string') {
      registerSelfWithRemoteToken(remoteId);
    }
  }
  // TODO(crbug.com/1440471): This should accept an Ack (see below).
}

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
function registerChildFrame(frame: HTMLIFrameElement): string {
  const remoteFrameId: string = generateRandomId();

  // TODO(crbug.com/1440471): Instead of a timeout, this should wait for an Ack
  // and resend until it gets one. The child frame may not yet be loaded.
  setTimeout(() => {
    if (frame.contentWindow) {
      frame.contentWindow.postMessage(
          {
            command: REGISTER_AS_CHILD_FRAME_COMMAND,
            remoteFrameId: remoteFrameId,
          },
          '*');
    }
  }, INTERFRAME_MESSAGE_DELAY_MS);

  return remoteFrameId;
}

// TODO(crbug.com/40263245): This is exposed via gCrWeb to enable use in
// form_handlers.js. When that file is converted to TS, this can be removed.
gCrWeb.child_frame_registration = {processChildFrameMessage};

export {registerChildFrame, processChildFrameMessage};
