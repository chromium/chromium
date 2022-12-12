/*
 * Copyright 2020 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

let methodName;

self.addEventListener('canmakepayment', (evt) => {
  evt.respondWith(true);
});

self.addEventListener('paymentrequest', (evt) => {
  methodName = evt.methodData[0].supportedMethods;
  evt.respondWith(new Promise((responder) => {
    const payerName = (evt.paymentOptions &&
      evt.paymentOptions.requestPayerName) ? 'John Smith' : '';
    responder({methodName, details: {status: 'success'}, payerName});
  }));
});
