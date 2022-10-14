/*
 * Copyright 2022 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Launches the PaymentRequest UI with the given payment method(s) and a
 * modifier for the second one.
 *
 * @param {sequence<PaymentMethodData>} methodData - An array of payment method
 *        objects.
 */
function modifierToSecondaryMethod(methodData) {
  try {
    new PaymentRequest(methodData, {
      total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}},
      modifiers: [
        {
          supportedMethods: methodData[1].supportedMethods,
          total: {
              label: 'Total',
              amount: {currency: 'USD', value: '4.00'},
          },
          additionalDisplayItems: [{
              label: 'A discount',
              amount: {currency: 'USD', value: '-1.00'},
          }],
          data: {discountProgramParticipantId: '86328764873265'},
        },
      ],
    })
        .show()
        .then(function(resp) {
          resp.complete('success')
              .then(function() {
                print(JSON.stringify(resp, undefined, 2));
              })
              .catch(function(error) {
                print(error.message);
              });
        })
        .catch(function(error) {
          print(error.message);
        });
  } catch (error) {
    print(error.message);
  }
}

/**
 * Launches the PaymentRequest UI with the given payment method(s) and a
 * modifier for the first one, but the modifier does not have a total specified.
 *
 * @param {sequence<PaymentMethodData>} methodData - An array of payment method
 *        objects.
 */
function modifierWithNoTotal(methodData) {
  try {
    new PaymentRequest(methodData, {
      total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}},
      modifiers: [
        {
          supportedMethods: methodData[0].supportedMethods,
          data: {
            methodParticipantId: '86328764873265',
          },
        },
      ],
    })
        .show()
        .then(function(resp) {
          resp.complete('success')
              .then(function() {
                print(JSON.stringify(resp, undefined, 2));
              })
              .catch(function(error) {
                print(error.message);
              });
        })
        .catch(function(error) {
          print(error.message);
        });
  } catch (error) {
    print(error.message);
  }
}
