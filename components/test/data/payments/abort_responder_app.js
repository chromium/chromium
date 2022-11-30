/*
 * Copyright 2019 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// An app that notifies the merchant that it was invoked, so it can be aborted.
// Should be used in conjunction with payment_handler_aborter.js.

let abortResponse = true;

self.addEventListener('canmakepayment', (event) => {
  event.respondWith(true);
});

self.addEventListener('abortpayment', (event) => {
  event.respondWith(abortResponse);
});

self.addEventListener('paymentrequest', (event) => {
  abortResponse = event.methodData[0].data.abortResponse;
  event.respondWith(new Promise(function() {
    event.changePaymentMethod(
        event.methodData[0].supportedMethods, {status: 'ready for abort'});
  }));
});
