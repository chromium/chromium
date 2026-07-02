/*
 * Copyright 2016 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

const defaultDetails = {
  total: {
    label: 'Total',
    amount: {
      currency: 'USD',
      value: '5.00',
    },
  },
};

/**
 * Calls PaymentRequest.show() and prints out the result.
 * @param {sequence<PaymentMethodData>} methodData The supported methods.
 * @private
 */
function buyHelper(methodData) {
  try {
    new PaymentRequest(methodData, defaultDetails)
        .show()
        .then(function(response) {
          response.complete('success')
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

