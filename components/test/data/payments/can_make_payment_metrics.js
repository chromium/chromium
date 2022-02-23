/*
 * Copyright 2017 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* global PaymentRequest:false */
/* global print:false */

// Global variable. Used by abort().
var request;

const bobPayMethod = Object.freeze({
  supportedMethods: 'https://bobpay.com',
});

const visaMethod = Object.freeze({
  supportedMethods: 'basic-card',
  data: {
    supportedNetworks: ['visa'],
  },
});

const kylePayMethod = Object.freeze({
  supportedMethods: 'https://kylepay.com/webpay',
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
function noQueryShow() { // eslint-disable-line no-unused-vars, max-len
  noQueryShowWithMethods([bobPayMethod, visaMethod]);
}

/**
 * Do not query CanMakePayment before showing the Payment Request. This request
 * will be sent with url-based methods only.
 */
 function noQueryShowWithUrlMethods() { // eslint-disable-line no-unused-vars
  noQueryShowWithMethods([bobPayMethod, kylePayMethod]);
}

/**
 * Do not query CanMakePayment before showing the Payment Request. This request
 * will be sent with the given methods.
 * @param {Array<Object>} methods An array of payment method objects.
 * @return {string} 'success' if show() has been successfully called; otherwise,
 *         return the error message.
 */
function noQueryShowWithMethods(methods) { // eslint-disable-line no-unused-vars
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
async function queryShow() { // eslint-disable-line no-unused-vars, max-len
  queryShowWithMethods([bobPayMethod, visaMethod]);
}

/**
 * Queries CanMakePayment and the shows the PaymentRequest after. This request
 * will be sent with url-based methods only.
 */
async function queryShowWithUrlMethods() { // eslint-disable-line no-unused-vars
  queryShowWithMethods([bobPayMethod, kylePayMethod]);
}

/**
 * Queries CanMakePayment and the shows the PaymentRequest after. This request
 * will be sent with url-based methods only.
 * @param {Array<Object>} methods An array of payment method objects.
 * @return {string} 'success' if show() has been successfully called; otherwise,
 *         return the error message.
 */
 async function queryShowWithMethods(methods) { // eslint-disable-line no-unused-vars, max-len
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
 async function queryShowWithMethodsBlocking(methods) { // eslint-disable-line no-unused-vars, max-len
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
async function queryNoShow() { // eslint-disable-line no-unused-vars, max-len
  queryNoShowWithMethods([bobPayMethod, visaMethod]);
}

/**
 * Queries CanMakePayment but does not show the PaymentRequest after. This
 * request will be sent with the given methods.
 */
async function queryNoShowWithUrlMethods() { // eslint-disable-line no-unused-vars, max-len
  queryNoShowWithMethods([bobPayMethod, kylePayMethod]);
}

/**
 * Queries CanMakePayment but does not show the PaymentRequest after. This
 * request will be sent with url-based methods only.
 * @param {Array<Object>} methods An array of payment method objects.
 * @return {Promise<string>} 'success' if the execution is successful;
 *         otherwise, returns the cause of the failure.
 */
async function queryNoShowWithMethods(methods) { // eslint-disable-line no-unused-vars, max-len
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
function abort() { // eslint-disable-line no-unused-vars
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
