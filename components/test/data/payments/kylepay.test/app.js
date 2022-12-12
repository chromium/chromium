/*
 * Copyright 2018 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

self.addEventListener('canmakepayment', (evt) => {
  evt.respondWith(true);
});

self.addEventListener('paymentrequest', (evt) => {
  evt.respondWith({
    methodName: evt.methodData[0].supportedMethods,
    details: {transactionId: '123'},
    shippingAddress: evt.paymentOptions.requestShipping
        ? {
            city: 'Reston',
            country: 'US',
            dependentLocality: '',
            organization: 'Google',
            phone: '+15555555555',
            postalCode: '20190',
            recipient: 'Jon Doe',
            region: 'VA',
            sortingCode: '',
            addressLine: [
                '1875 Explorer St #1000',
            ],
        }
        : {},
    shippingOption: evt.paymentOptions.requestShipping
        ? evt.shippingOptions[0].id
        : '',
    payerName: evt.paymentOptions.requestPayerName ? 'Bob' : '',
    payerEmail: evt.paymentOptions.requestPayerEmail ? 'bob@gmail.com' : '',
    payerPhone: evt.paymentOptions.requestPayerPhone ? '+15555555555' : '',
  });
});
