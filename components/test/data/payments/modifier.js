/*
 * Copyright 2017 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Invokes the PaymentRequest with a modifier that contains the bare mininum of
 * required fields.
 */
function buy() {
  try {
    new PaymentRequest(
        [{
          supportedMethods: 'foo',
        }],
        {
          total: {
            label: 'Total',
            amount: {
              currency: 'USD',
              value: '5.00',
            },
          },
          modifiers: [{
            supportedMethods: 'foo',
          }],
        })
        .show()
        .then(function(response) {
          response.complete()
              .then(function() {
                print(complete);
              })
              .catch(function(error) {
                print(error);
              });
        })
        .catch(function(error) {
          print(error);
        });
  } catch (error) {
    print(error.message);
  }
}
