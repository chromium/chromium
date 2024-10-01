/*
 * Copyright 2017 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// Global variable. Used by abort().
let request;

const bobPayMethod = Object.freeze({
  supportedMethods: 'https://bobpay.test',
});

const visaMethod = Object.freeze({
  supportedMethods: 'basic-card',
  data: {
    supportedNetworks: ['visa'],
  },
});

const kylePayMethod = Object.freeze({
  supportedMethods: 'https://kylepay.test/webpay',
});

const defaultDetails = Object.freeze({
  total: {
    label: 'Total',
    amount: {
      currency: 'USD',
      value: '5.00',
    },
  },
});

/**
 * Do not query CanMakePayment before showing the Payment Request. This request
 * will be sent with a url-based method and a basic-card methods.
 */
function noQueryShow() {
  noQueryShowWithMethods([bobPayMethod, visaMethod]);
}

/**
 * Do not query CanMakePayment before showing the Payment Request. This request
 * will be sent with url-based methods only.
 */
function noQueryShowWithUrlMethods() {
  noQueryShowWithMethods([bobPayMethod, kylePayMethod]);
}

/**
 * Do not query CanMakePayment before showing the Payment Request. This request
 * will be sent with the given methods.
 * @param {Array<Object>} methods An array of payment method objects.
 * @return {string} 'success' if show() has been successfully called; otherwise,
 *         return the error message.
 */
function noQueryShowWithMethods(methods) {
  try {
    request = new PaymentRequest(methods, defaultDetails);
    request.show()
        .then(function(resp) {
          resp.complete('success')
              .then(function() {
                print(JSON.stringify(resp, undefined, 2));
              })
              .catch(function(error) {
                print(error);
              });
        })
        .catch(function(error) {
          print(error);
        });
    return 'success';
  } catch (error) {
    print(error.message);
    return error.message;
  }
}

/**
 * Queries CanMakePayment and the shows the PaymentRequest after. This request
 * will be sent with a url-based method and a basic-card methods.
 */
async function queryShow() {
  queryShowWithMethods([bobPayMethod, visaMethod]);
}

/**
 * Queries CanMakePayment and the shows the PaymentRequest after. This request
 * will be sent with url-based methods only.
 */
async function queryShowWithUrlMethods() {
  queryShowWithMethods([bobPayMethod, kylePayMethod]);
}

/**
 * Queries CanMakePayment and the shows the PaymentRequest after. This request
 * will be sent with url-based methods only.
 * @param {Array<Object>} methods An array of payment method objects.
 * @return {string} 'success' if show() has been successfully called; otherwise,
 *         return the error message.
 */
async function queryShowWithMethods(methods) {
  try {
    request = new PaymentRequest(methods, defaultDetails);
    print(await request.canMakePayment());
    print(await request.hasEnrolledInstrument());
    request.show()
        .then(function(resp) {
          resp.complete('success')
              .then(function() {
                print(JSON.stringify(resp, undefined, 2));
              })
              .catch(function(error) {
                print(error);
              });
        })
        .catch(function(error) {
          print(error);
        });
    return 'success';
  } catch (error) {
    print(error.message);
    return error.message;
  }
}

/**
 * Queries CanMakePayment, HasEnrolledInstrument, and shows the PaymentRequest.
 * If called with 'await', this method will be blocked until all of the
 * promises are resolved.
 * @param {Array<Object>} methods An array of payment method objects.
 * @return {Promise<string>} 'success' if the execution is successful;
 *         otherwise, returns the cause of the failure.
 */
async function queryShowWithMethodsBlocking(methods) {
  try {
    request = new PaymentRequest(methods, defaultDetails);
    print(await request.canMakePayment());
    print(await request.hasEnrolledInstrument());
    const resp = await request.show();
    print(JSON.stringify(resp, undefined, 2));
    await resp.complete('success');
    return 'success';
  } catch (error) {
    print(error.message);
    return error.message;
  }
}

/**
 * Queries CanMakePayment but does not show the PaymentRequest after. This
 * request will be sent with a url-based method and a basic-card methods.
 */
async function queryNoShow() {
  queryNoShowWithMethods([bobPayMethod, visaMethod]);
}

/**
 * Queries CanMakePayment but does not show the PaymentRequest after. This
 * request will be sent with the given methods.
 */
async function queryNoShowWithUrlMethods() {
  queryNoShowWithMethods([bobPayMethod, kylePayMethod]);
}

/**
 * Queries CanMakePayment but does not show the PaymentRequest after. This
 * request will be sent with url-based methods only.
 * @param {Array<Object>} methods An array of payment method objects.
 * @return {Promise<string>} 'success' if the execution is successful;
 *         otherwise, returns the cause of the failure.
 */
async function queryNoShowWithMethods(methods) {
  try {
    request = new PaymentRequest(methods, defaultDetails);
    print(await request.canMakePayment());
    print(await request.hasEnrolledInstrument());
    return 'success';
  } catch (error) {
    print(error.message);
    return error.message;
  }
}

/**
 * Aborts the PaymentRequest UI.
 */
function abort() {
  try {
    request.abort()
        .then(function() {
          print('Aborted');
        })
        .catch(function() {
          print('Cannot abort');
        });
  } catch (error) {
    print(error.message);
  }
}
