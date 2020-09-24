// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * A script for the app inside the iframe. Implements a delegate.
 */

/** A pipe through which we can send messages to the parent frame. */
const parentMessagePipe = new MessagePipe('chrome://help-app', window.parent);

/**
 * A delegate which exposes privileged WebUI functionality to the help
 * app.
 * @type {!helpApp.ClientApiDelegate}
 */
const DELEGATE = {
  async openFeedbackDialog() {
    const response =
        await parentMessagePipe.sendMessage(Message.OPEN_FEEDBACK_DIALOG);
    return /** @type {?string} */ (response['errorMessage']);
  },
  async showParentalControls() {
    await parentMessagePipe.sendMessage(Message.SHOW_PARENTAL_CONTROLS);
  },
  // TODO(b/166047521): Complete the implementation of these.
  async addOrUpdateSearchIndex() {
    return;
  },
  async clearSearchIndex() {
    return;
  },
  async findInSearchIndex() {
    return {results: null};
  },
};

/**
 * Returns the help app if it can find it in the DOM.
 * @return {?helpApp.ClientApi}
 */
function getApp() {
  return /** @type {?helpApp.ClientApi} */ (
      document.querySelector('showoff-app'));
}

/**
 * Runs any initialization code on the help app once it is in the dom.
 * @param {!helpApp.ClientApi} app
 */
function initializeApp(app) {
  app.setDelegate(DELEGATE);
}

/**
 * Called when a mutation occurs on document.body to check if the help app is
 * available.
 * @param {!Array<!MutationRecord>} mutationsList
 * @param {!MutationObserver} observer
 */
function mutationCallback(mutationsList, observer) {
  const app = getApp();
  if (!app) {
    return;
  }
  // The help app now exists so we can initialize it.
  initializeApp(app);
  observer.disconnect();
}

window.addEventListener('DOMContentLoaded', () => {
  const app = getApp();
  if (app) {
    initializeApp(app);
    return;
  }
  // If translations need to be fetched, the app element may not be added yet.
  // In that case, observe <body> until it is.
  const observer = new MutationObserver(mutationCallback);
  observer.observe(document.body, {childList: true});
});
