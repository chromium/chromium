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
 * Identifier for the registration ack message.
 */
const REGISTER_AS_CHILD_FRAME_ACK = 'registerAsChildFrameAck';

/**
 * Maximal number of registration attempts per token. This value is also used
 * to signal that the registration was acknowledged hence do not try any further
 * attempt.
 */
const MAX_REGISTRATION_ATTEMPTS = 9;

/**
 * Number that represents a registered frame.
 */
const FRAME_REGISTERED = MAX_REGISTRATION_ATTEMPTS + 1;

/**
 * How long to wait before first posting a message to the child frame, to
 * improve the chance that it actually loads before the message is sent.
 */
const INTERFRAME_MESSAGE_DELAY_MS = 100;

/**
 * Initial delay for the registration watchdog retry.
 */
const WATCHDOG_INITIAL_RETRY_DELAY_MS = 50;

/**
 * Maximum capacity of registration records in the log book.
 */
const REGISTRATION_LOGBOOK_MAX_CAPACITY = 100;

/**
 * A logbook for remote token registration mapping each remote token to the
 * number of registration attempts done so far with the corresponding child
 * frame. Persists the information during all lifetime of the frame so no-op
 * re-registrations are not attempted, saving unnecessary interprocess calls.
 *
 * This logbook only tracks registered child frames towards this frame. Meaning
 * that the parent frame will not be in the logbook of the child frame.
 */
const registrationLogbook: Map<string, number> = new Map();

/**
 * Updates `count` of the corresponding `remoteToken` in the registration
 * logbook iff the maximal capacity wasn't reached.
 * @param remoteToken The remote token to update.
 * @param count The new attempts count for the token.
 */
function updateRegistrationLogbook(remoteToken: string, count: number) {
  if (registrationLogbook.size >= REGISTRATION_LOGBOOK_MAX_CAPACITY) {
    return;
  }

  registrationLogbook.set(remoteToken, count);
}

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
  const command: unknown = payload.data?.command;
  if (command === REGISTER_AS_CHILD_FRAME_COMMAND) {
    const remoteId = payload.data?.remoteFrameId;
    if (typeof remoteId === 'string') {
      registerSelfWithRemoteToken(remoteId);
      // Send an ack back to the sender.
      payload.source?.postMessage({
        command: REGISTER_AS_CHILD_FRAME_ACK,
        remoteFrameId: remoteId,
      });
    }
  } else if (command === REGISTER_AS_CHILD_FRAME_ACK) {
    const remoteId = payload.data?.remoteFrameId;
    if (typeof remoteId === 'string') {
      // Registration done, signal that there shouldn't be any further attempt
      // to register that token.
      updateRegistrationLogbook(remoteId, FRAME_REGISTERED);
    }
  }
}

/**
 * Gets the remote ID of the corresponding `frame`. Caches the remote ID of each
 * frame to avoid registering the same frame more than once.
 * @param frame The frame to get the remote ID for.
 * @returns The remote ID for the frame. Will either return the ID that was
 *      cached or a freshly generated one.
 */
function getRemoteIdForFrame(frame: HTMLIFrameElement): string {
  if (!gCrWeb.hasOwnProperty('remoteFrameIdRegistrar')) {
    gCrWeb.remoteFrameIdRegistrar = new Map();
  }

  // Return the cached remote token if the frame was already registered.
  if (gCrWeb.remoteFrameIdRegistrar.has(frame)) {
    return gCrWeb.remoteFrameIdRegistrar.get(frame);
  }

  // Otherwise, create a remote ID for the frame and cache it.
  const remoteId: string = generateRandomId();
  gCrWeb.remoteFrameIdRegistrar.set(frame, remoteId);
  return remoteId;
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
  const remoteFrameId: string = getRemoteIdForFrame(frame);

  const register = (delayUntilNextRetryMs: number) => {
    if ((registrationLogbook.get(remoteFrameId) ?? 0) >=
            MAX_REGISTRATION_ATTEMPTS ||
        registrationLogbook.size >= REGISTRATION_LOGBOOK_MAX_CAPACITY) {
      // Stop attempting registrations if the maximal number of attempts was
      // reached or the logbook is full. Reaching that count may also mean
      // that the ack was received so no need to retry a registration in that
      // case either.
      return;
    }

    if (frame.contentWindow) {
      // Increment or create new log entry for attempts.
      updateRegistrationLogbook(
          remoteFrameId, (registrationLogbook.get(remoteFrameId) ?? 0) + 1);
      frame.contentWindow.postMessage(
          {
            command: REGISTER_AS_CHILD_FRAME_COMMAND,
            remoteFrameId: remoteFrameId,
          },
          '*');
      // Set a watch dog that will retry a registration if the frame didn't
      // respond and the attempts limit wasn't reached yet. Give some time to
      // the frame to respond before checking. Double the delay between retries
      // at each retry as exponential backoff.
      setTimeout(
          () => register(delayUntilNextRetryMs * 2), delayUntilNextRetryMs);
    }
  };

  setTimeout(
      () => register(WATCHDOG_INITIAL_RETRY_DELAY_MS),
      INTERFRAME_MESSAGE_DELAY_MS);

  return remoteFrameId;
}

// TODO(crbug.com/40263245): This is exposed via gCrWeb to enable use in
// form_handlers.js. When that file is converted to TS, this can be removed.
gCrWeb.child_frame_registration = {processChildFrameMessage};

export {
  registerChildFrame,
  registerSelfWithRemoteToken,
  processChildFrameMessage,
};
