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
const guestFrame = /** @type {!HTMLIFrameElement} */ (
    document.createElement('iframe'));
guestFrame.src = `${GUEST_ORIGIN}${location.pathname}`;
document.body.appendChild(guestFrame);

/**
 * A pipe through which we can send messages to the guest frame.
 * Use an undefined `target` to find the <iframe> automatically.
 * Do not rethrow errors, since handlers installed here are expected to
 * throw exceptions that are handled on the other side of the pipe (in the guest
 * frame), not on this side.
 */
const guestMessagePipe =
    new MessagePipe('chrome-untrusted://help-app', /*target=*/ undefined,
        /*rethrowErrors=*/ false);

guestMessagePipe.registerHandler(Message.OPEN_FEEDBACK_DIALOG, () => {
  return help_app.handler.openFeedbackDialog();
});

guestMessagePipe.registerHandler(Message.SHOW_PARENTAL_CONTROLS, () => {
  help_app.handler.showParentalControls();
});
