/*
 * Copyright 2019 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

let method = null;
let respond = null;

self.addEventListener('canmakepayment', (evt) => {
  evt.respondWith(true);
});

self.addEventListener('message', (evt) => {
  respond({methodName: method, details: {status: 'success'}});
});

self.addEventListener('paymentrequest', (evt) => {
  method = evt.methodData[0].supportedMethods;
  evt.respondWith(new Promise((responder) => {
    respond = responder;
    evt.openWindow('payment_handler_window.html');
  }));
});
