/*
 * Copyright 2019 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

self.addEventListener('canmakepayment', (event) => {
  event.respondWith(true);
});

/**
 * Responds to the PaymentRequest |event| by calling its changePaymentMethod()
 * method and returning its result, thus allowing for testing without user
 * interaction with a skip-UI flow.
 *
 * @param {PaymentRequestEvent} event - The event to respond.
 * @return {PamentDetailsUpdate} - The update to the payment details.
 */
async function responder(event) {
  const methodName = event.methodData[0].supportedMethods;
  if (!event.changePaymentMethod) {
    return {
      methodName,
      details: {
        changePaymentMethodReturned:
          'The changePaymentMethod() method is not implemented.',
      },
    };
  }
  let changePaymentMethodReturned;
  try {
    const response = await event.changePaymentMethod(methodName);
    changePaymentMethodReturned = response;
  } catch (error) {
    changePaymentMethodReturned = error.message;
  }
  return {methodName, details: {changePaymentMethodReturned}};
}

self.addEventListener('paymentrequest', (event) => {
  event.respondWith(responder(event));
});
