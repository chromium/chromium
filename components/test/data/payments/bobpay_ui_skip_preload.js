/*
 * Copyright 2017 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Builds PaymentRequest for Bob Pay, but does not show any UI yet.
 *
 * @return {PaymentRequest} The PaymentRequest object.
 */
function initPaymentRequest() {
  const supportedInstruments = [{
    supportedMethods: 'https://bobpay.test',
  }];

  const details = {
    total: {
      label: 'Donation',
      amount: {
        currency: 'USD',
        value: '55.00',
      },
    },
    displayItems: [
      {
        label: 'Original donation amount',
        amount: {
          currency: 'USD',
          value: '65.00',
        },
      },
      {
        label: 'Friends and family discount',
        amount: {
          currency: 'USD',
          value: '-10.00',
        },
      },
    ],
  };

  return new PaymentRequest(supportedInstruments, details);
}

/**
 * Launches the PaymentRequest UI with Bob Pay as the only payment method.
 * Preloads the second instance of PaymentRequest while the first instance is
 * showing.
 */
function buy() {
  let request = initPaymentRequest();
  request.show()
      .then(function(instrumentResponse) {
        window.setTimeout(function() {
          instrumentResponse.complete('success')
              .then(function() {
                print(JSON.stringify(instrumentResponse, undefined, 2));
              })
              .catch(function(err) {
                print(err);
              });
        }, 500);
      })
      .catch(function(err) {
        print(err);
      });
  request = initPaymentRequest();
}
