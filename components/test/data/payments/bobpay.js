/*
 * Copyright 2016 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Builds a payment request with URL based payment methods.
 * @return {!PaymentRequest} A payment request with URL based payment methods.
 * @private
 */
function buildPaymentRequest() {
  return new PaymentRequest(
      [
        {supportedMethods: 'https://bobpay.test'},
        {supportedMethods: 'https://alicepay.test'},
      ],
      {total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}}});
}

/**
 * Launches the PaymentRequest UI with Bob Pay as one of multiple payment
 * methods. The result or error is both printed and returned as the Java test
 * reads it from the page and the C++ browser test reads the return value.
 * @return {string} - the response or error string.
 */
async function buy() {
  let responseString;
  try {
    const resp = await buildPaymentRequest().show();
    await resp.complete('success');
    responseString = resp.methodName + '\n' +
        JSON.stringify(resp.details, undefined, 2);
  } catch (error) {
    responseString = error.toString();
  }
  print(responseString);
  return responseString;
}

/**
 * Queries CanMakePayment but does not show the PaymentRequest after. The result
 * or error is both printed and returned as the Java test reads it from the page
 * and the C++ browser test reads the return value.
 * @return {string} - the result or error string.
 */
async function canMakePayment() {
  try {
    const result = await buildPaymentRequest().canMakePayment();
    print(result);
    return result.toString();
  } catch (error) {
    print('exception thrown<br>' + error);
    return error.toString();
  }
}
