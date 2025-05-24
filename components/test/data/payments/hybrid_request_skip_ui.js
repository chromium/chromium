/*
 * Copyright 2019 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Creates a new PaymentRequest.
 *
 * @param {Object} params Specifies overrides used to create the request.
 * @return {PaymentRequest} A new PaymentRequest.
 */
function makeRequest(params) {
  const basicCard = {supportedMethods: 'basic-card'};

  const defaultGPayData = {
    apiVersion: params['apiVersion'],
    allowedPaymentMethods: [{
      type: 'CARD',
    }],
  };

  const gpay = {
    supportedMethods: 'https://google.com/pay',
    data: Object.assign(defaultGPayData, params['gpayData']),
  };

  const details = {
    total: {
      label: 'Total',
      amount: {currency: 'USD', value: '1.0'},
    },
    shippingOptions: [{
      id: 'free-shipping',
      label: 'Free Shipping',
      amount: {currency: 'USD', value: '0.0'},
      selected: true,
    }],
  };
  const options = {
    requestPayerName: params['requestName'],
    requestPayerEmail: params['requestEmail'],
    requestPayerPhone: params['requestPhone'],
    requestShipping: params['requestShipping'],
  };
  return new PaymentRequest([basicCard, gpay], details, options);
}

/**
 * Main test entry point.
 *
 * @param {Object} params - Specifies overrides for creating the PaymentRequest.
 * @return {string} A stringified result of the PaymentResponse or an error.
 *     This is tested against expectations in the browsertest.
 */
async function buy(params) {
  const request = makeRequest(params || {});
  const result = await request.show()
                     .then((response) => {
                       response.complete();
                       return JSON.stringify({
                         details: response.details,
                         shippingAddress: response.shippingAddress,
                         shippingOption: response.shippingOption,
                         payerName: response.payerName,
                         payerEmail: response.payerEmail,
                         payerPhone: response.payerPhone,
                       });
                     })
                     .catch((error) => {
                       return 'showPromise error: ' + error.message;
                     });
  return result;
}
