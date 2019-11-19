/*
 * Copyright 2017 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Builds a payment request for a debit card.
 * @return {!PaymentRequest} A payment request for a debit card.
 * @private
 */
function buildPaymentRequest() {
  return new PaymentRequest(
      [{
        supportedMethods: 'basic-card',
        data: {
          supportedTypes: ['debit'],
        },
      }],
      {
        total: {
          label: 'Total',
          amount: {
            currency: 'USD',
            value: '1.00',
          },
        },
      });
}

/** Requests payment via a debit card. */
function buy() { // eslint-disable-line no-unused-vars
  try {
    buildPaymentRequest()
        .show()
        .then(function(response) {
          response.complete()
              .then(function() {
                print(JSON.stringify(response, undefined, 2));
              })
              .catch(function(error) {
                print(error);
              });
        })
        .catch(function(error) {
          print(error);
        });
  } catch (error) {
    print(error);
  }
}

/** Checks whether payment via a debit card is possible. */
function canMakePayment() { // eslint-disable-line no-unused-vars
  try {
    buildPaymentRequest()
        .canMakePayment()
        .then(function(result) {
          print(result);
        })
        .catch(function(error) {
          print(error);
        });
  } catch (error) {
    print(error);
  }
}

/** Checks whether an active debit card is ready for payment. */
function hasEnrolledInstrument() { // eslint-disable-line no-unused-vars
  try {
    buildPaymentRequest()
        .hasEnrolledInstrument()
        .then(function(result) {
          print(result);
        })
        .catch(function(error) {
          print(error);
        });
  } catch (error) {
    print(error);
  }
}
