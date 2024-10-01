/*
 * Copyright 2018 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Pops up a payment sheet, allowing options to be
 * passed in if particular values are needed.
 *
 * @param {PaymentOptions?} options Payment options
 * @return {Promise<PaymentResponse>} Payment response
 */
function getPaymentResponse(options) {
  return getPaymentResponseWithMethod(
      options, [{supportedMethods: 'basic-card'}]);
}

/**
 * Pops up a payment sheet, allowing options to be
 * passed in if particular values are needed.
 *
 * @param {PaymentOptions?} options Payment options
 * @param {Array<Object>} methodData An array of payment method objects used as
 *        the first parameter of the PaymentRequest API.
 * @return {Promise<PaymentResponse>} Payment response
 */
function getPaymentResponseWithMethod(options, methodData) {
  const details = {
    total: {
      label: 'Total',
      amount: {
        currency: 'USD',
        value: '5.00',
      },
    },
    shippingOptions: [{
      id: 'freeShippingOption',
      label: 'Free global shipping',
      selected: true,
      amount: {
        currency: 'USD',
        value: '0',
      },
    }],
  };

  const request = new PaymentRequest(methodData, details, options);
  request.onshippingaddresschange = function(e) {
    e.updateWith(details);
  };
  request.onshippingoptionchange = function(e) {
    e.updateWith(details);
  };
  return request.show();
}
