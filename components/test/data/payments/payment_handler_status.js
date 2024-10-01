/*
 * Copyright 2020 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Returns the status field from the payment handler's response for the given
 * payment method identifier.
 * @param {string} method - The payment method identifier to use.
 * @return {string} - The status field or error message.
 */
async function getStatus(method) {
  return getStatusForMethodData([{supportedMethods: method}]);
}

/**
 * Returns the status field from the payment handler's response for the given
 * list of payment method identifiers.
 * @param {array<string>} methods - The list of payment methods to use.
 * @return {string} - The status field or error message.
 */
async function getStatusList(methods) {
  const methodData = [];
  for (const method of methods) {
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
  return getStatusForMethodDataAfterCanMakePayment(methodData, false);
}

/**
 * Returns the status field from the payment handler's response for given
 * payment method data.
 * @param {array<PaymentMethodData>} methodData - The method data to use.
 * @param {bool} checkCanMakePaymentFirst - Whether to wait for canMakePayment()
 * to complete before invoking show(). The return value of canMakePayment() is
 * ignored.
 * @return {string} - The status field or error message.
 */
async function getStatusForMethodDataAfterCanMakePayment(
    methodData, checkCanMakePaymentFirst) {
  try {
    const request = new PaymentRequest(
        methodData,
        {total: {label: 'TEST', amount: {currency: 'USD', value: '0.01'}}});
    if (checkCanMakePaymentFirst) {
      await request.canMakePayment(); // Ignore the result.
    }
    const response = await request.show();
    await response.complete();
    if (!response.details.status) {
      return 'Payment handler did not specify the status.';
    }
    return response.details.status;
  } catch (e) {
    return e.toString();
  }
}

/**
 * Returns the status field from the payment handler's response for given
 * payment method data. Passes a promise into PaymentRequest.show() to delay
 * initialization by 1 second.
 * @param {array<PaymentMethodData>} methodData - The method data to use.
 * @return {string} - The status field or error message.
 */
async function getStatusForMethodDataWithShowPromise(methodData) {
  try {
    const details = {total: {label: 'TEST',
        amount: {currency: 'USD', value: '0.01'}}};
    const request = new PaymentRequest(methodData, details);
    const response = await request.show(new Promise((resolve) => {
      window.setTimeout(() => resolve(details), 1000);
    }));
    await response.complete();
    if (!response.details.status) {
      return 'Payment handler did not specify the status.';
    }
    return response.details.status;
  } catch (e) {
    return e.toString();
  }
}

/**
 * Returns the status field from the payment handler's response for given
 * payment method data. Passes an empty Promise.resolve({}) promise into
 * PaymentRequest.show().
 * @param {array<PaymentMethodData>} methodData - The method data to use.
 * @return {string} - The status field or error message.
 */
async function getStatusForMethodDataWithEmptyShowPromise(methodData) {
  try {
    const details = {total: {label: 'TEST',
        amount: {currency: 'USD', value: '0.01'}}};
    const request = new PaymentRequest(methodData, details);
    const response = await request.show(Promise.resolve({}));
    await response.complete();
    if (!response.details.status) {
      return 'Payment handler did not specify the status.';
    }
    return response.details.status;
  } catch (e) {
    return e.toString();
  }
}
