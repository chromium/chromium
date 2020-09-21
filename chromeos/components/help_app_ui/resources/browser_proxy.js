// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const help_app = {
  handler: new helpAppUi.mojom.PageHandlerRemote()
};

// Set up a page handler to talk to the browser process.
helpAppUi.mojom.PageHandlerFactory.getRemote().createPageHandler(
    help_app.handler.$.bindNewPipeAndPassReceiver());

// Set up an index remote to talk to Local Search Service.
// TODO(b/166047521): Define and use API to make calls to this from untrusted
// context.
const indexRemote = chromeos.localSearchService.mojom.IndexProxy.getRemote();

const GUEST_ORIGIN = 'chrome-untrusted://help-app';
const guestFrame = /** @type{HTMLIFrameElement} */ (
    document.createElement('iframe'));
guestFrame.src = `${GUEST_ORIGIN}${location.pathname}`;
document.body.appendChild(guestFrame);

/**
 * Handles messages from the untrusted context.
 * @param {Event} event
 */
function receiveMessage(event) {
  const msgEvent = /** @type{MessageEvent<string>} */ (event);
  if (msgEvent.origin !== GUEST_ORIGIN) {
    return;
  }

  switch (msgEvent.data) {
    case 'feedback':
      help_app.handler.openFeedbackDialog().then(response => {
        guestFrame.contentWindow.postMessage(response, GUEST_ORIGIN);
      });
      break;
    case 'show-parental-controls':
      help_app.handler.showParentalControls();
      break;
  }
}
window.addEventListener('message', receiveMessage, false);
