/*
 * Copyright 2019 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

self.addEventListener('canmakepayment', (event) => {
  event.respondWith(true);
});

/**
 * Responds to the PaymentRequest |event| by calling its changeShippingOption()
 * method and returning its result, thus allowing for testing without user
 * interaction with a skip-UI flow.
 *
 * @param {PaymentRequestEvent} event - The event to respond.
 * @return {PamentDetailsUpdate} - The update to the payment details.
 */
async function responder(event) {
  const methodName = event.methodData[0].supportedMethods;
  const shippingOption = event.shippingOptions[0].id;
  const shippingAddress = {
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
  };

  let changeShippingReturnedValue;
  if (!event.changeShippingOption) {
    return {
      methodName,
      details: {
        changeShippingReturnedValue:
            'The changeShippingOption() method is not implemented.',
      },
    };
  }
  try {
    const response = await event.changeShippingOption(shippingOption);
    changeShippingReturnedValue = response;
  } catch (err) {
    changeShippingReturnedValue = err.message;
  }
  return {
    methodName,
    details: {changeShippingReturnedValue},
    shippingAddress,
    shippingOption,
  };
}

self.addEventListener('paymentrequest', (event) => {
  event.respondWith(responder(event));
});
