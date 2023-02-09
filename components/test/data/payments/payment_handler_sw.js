/*
 * Copyright 2019 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

let respond = null;
let requestEvent = null;

self.addEventListener('canmakepayment', (evt) => {
  evt.respondWith(true);
});

self.addEventListener('message', (evt) => {
  const methodName = requestEvent.methodData[0].supportedMethods;
  const shippingOption = (requestEvent.paymentOptions &&
                          requestEvent.paymentOptions.requestShipping) ?
      requestEvent.shippingOptions[0].id :
      '';
  const payerName = (requestEvent.paymentOptions &&
                     requestEvent.paymentOptions.requestPayerName) ?
      'John Smith' :
      '';
  const payerEmail = (requestEvent.paymentOptions &&
                      requestEvent.paymentOptions.requestPayerEmail) ?
      'smith@gmail.com' :
      '';
  const payerPhone = (requestEvent.paymentOptions &&
                      requestEvent.paymentOptions.requestPayerPhone) ?
      '+15555555555' :
      '';
  const shippingAddress = (requestEvent.paymentOptions &&
                           requestEvent.paymentOptions.requestShipping) ?
      {
        addressLine: [
          '1875 Explorer St #1000',
        ],
        city: 'Reston',
        country: 'US',
        dependentLocality: '',
        organization: 'Google',
        phone: '+15555555555',
        postalCode: '20190',
        recipient: 'John Smith',
        region: 'VA',
        sortingCode: '',
      } :
      {};

  respond({
    methodName,
    details: {status: 'success'},
    payerName,
    payerEmail,
    payerPhone,
    shippingAddress,
    shippingOption,
  });
});

self.addEventListener('paymentrequest', (evt) => {
  requestEvent = evt;
  evt.respondWith(new Promise((responder) => {
    respond = responder;
    paymentHandlerWindow = evt.methodData[0]['data'] !== undefined &&
            evt.methodData[0]['data']['windowPage'] !== undefined ?
        evt.methodData[0]['data']['windowPage'] :
        'payment_handler_window.html';
    evt.openWindow(paymentHandlerWindow);
  }));
});
