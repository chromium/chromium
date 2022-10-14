/*
 * Copyright 2020 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Launches a payment handler, waits for the payment handler to issue a payment
 * method change event with 'status': 'ready for abort' (as, for example, done
 * in abort_repsonder_app.js), then aborts it.
 * @param {string} method - The payment method identifier to use.
 * @param {boolean} abortResponse - Whether the app should be abortable.
 * @return {string} - Either 'Abort completed' or an error message.
 */
async function launchAndAbort(method, abortResponse) {
  try {
    const details = {
      total: {label: 'TEST', amount: {currency: 'USD', value: '0.01'}},
    };
    const request = new PaymentRequest(
        [{supportedMethods: method, data: {abortResponse}}], details);
    const eventPromise = new Promise((resolveEventPromise) => {
      request.addEventListener('paymentmethodchange', (event) => {
        event.updateWith(details);
        resolveEventPromise(event);
      });
    });
    const showRejectPromise = new Promise((resolveShowRejectPromise) => {
      request.show().catch(resolveShowRejectPromise);
    });
    const event = await eventPromise;
    if (event.methodDetails.status !== 'ready for abort') {
      return event.methodDetails.status;
    }
    await request.abort();
    await showRejectPromise;
    return 'Abort completed';
  } catch (e) {
    return e.message;
  }
}
