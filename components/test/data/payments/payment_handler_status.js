/*
 * Copyright 2020 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Returns the status field from the payment handler's response for the given
 * payment method identifier.
 * @param {string} method - The payment method identifier to use.
 * @return {string} - The status field or error message.
 */
async function getStatus(method) { // eslint-disable-line no-unused-vars
  return getStatusForMethodData([{supportedMethods: method}]);
}

/**
 * Returns the status field from the payment handler's response for the given
 * list of payment method identifiers.
 * @param {array<string>} methods - The list of payment methods to use.
 * @return {string} - The status field or error message.
 */
async function getStatusList(methods) { // eslint-disable-line no-unused-vars
  const methodData = [];
  for (let method of methods) {
    methodData.push({supportedMethods: method});
  }
  return getStatusForMethodData(methodData);
}

/**
 * Returns the status field from the payment handler's response for given
 * payment method data.
 * @param {array<PaymentMethodData>} methodData - The method data to use.
 * @return {string} - The status field or error message.
 */
async function getStatusForMethodData(methodData) {
  try {
    const request = new PaymentRequest(
        methodData,
        {total: {label: 'TEST', amount: {currency: 'USD', value: '0.01'}}});
    const response = await request.show();
    await response.complete();
    if (!response.details.status) {
      return 'Payment handler did not specify the status.';
    }
    return response.details.status;
  } catch (e) {
    return e.message;
  }
}
