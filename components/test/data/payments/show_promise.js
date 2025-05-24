/*
 * Copyright 2018 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Initializes the payment request object.
 * @return {PaymentRequest} - The newly initialized object.
 */
function buildPaymentRequest() {
  if (!window.PaymentRequest) {
    throw new Error('Payment Request API not available.');
  }

  const supportedInstruments = [
    {
      supportedMethods: 'basic-card',
    },
  ];

  const details = {
    total: {
      label: 'Donation',
      amount: {
        currency: 'USD',
        value: '1.00',
        pending: true,
      },
    },
  };

  return new PaymentRequest(supportedInstruments, details);
}

/**
 * Calls PaymentRequest.show() without a promise.
 */
function buyWithNoPromise() {
  try {
    request = buildPaymentRequest();
    print('The final donation amount is USD $1.00.');
    request.show()
        .then(function(resp) {
          resp.complete('success')
              .then(function() {
                print(JSON.stringify(resp, undefined, 2));
              })
              .catch(function(err) {
                print(err);
              });
        })
        .catch(function(err) {
          print(err);
        });
  } catch (err) {
    print(err);
  }
}

/**
 * Calls PaymentRequest.show() with a promise that resolves.
 */
function buyWithResolvingPromise() {
  try {
    const request = buildPaymentRequest();
    print('The initial donation amount is USD $1.00.');
    request
        .show(new Promise(function(resolve) {
          print('Calculating the final donation amount...');
          window.setTimeout(function() {
            print('Final donation amount is USD $0.99.');
            const details = {
              total: {
                label: 'Donation',
                amount: {
                  currency: 'USD',
                  value: '0.99',
                },
              },
            };

            resolve(details);
          }, 5000); // 5 seconds
        }))
        .then(function(instrumentResponse) {
          instrumentResponse.complete('success');
        })
        .catch(function(err) {
          print(err);
        });
  } catch (err) {
    print(err);
  }
}

/**
 * Calls PaymentRequest.show() with a promise that rejects.
 */
function buyWithRejectingPromise() {
  try {
    const request = buildPaymentRequest();
    print('The initial donation amount is USD $1.00.');
    request
        .show(new Promise(function(resolve, reject) {
          print('Calculating the final donation amount...');
          window.setTimeout(function() {
            reject('Unable to calculate final donation amount.');
          }, 5000); // 5 seconds
        }))
        .then(function(instrumentResponse) {
          instrumentResponse.complete('success');
        })
        .catch(function(err) {
          print(err);
        });
  } catch (err) {
    print(err);
  }
}
