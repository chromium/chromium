// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Checks canMakePayment() for the given payment method identifier.
 * @param {string} method - Payment method identifier to check.
 * @return {Promise<string>} - Either 'true', 'false', or an error message.
 */
async function checkCanMakePayment(method) {
  try {
    const request = new PaymentRequest(
      [
        {supportedMethods: method},
      ],
      {total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}}});
    const result = await request.canMakePayment();
    return (result ? 'true' : 'false');
  } catch (error) {
    return error.toString();
  }
}
