/*
 * Copyright 2019 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Queries Payment Request with some empty parameters.
 * Regression test for: https://crbug.com/1022810
 * @return {Promise<boolean>} - Whether a payment can be made.
 */
async function runTest() {
  return new PaymentRequest(
             [{supportedMethods: 'https://kylepay.test/webpay'}], {
               displayItems: [],
               id: '',
               modifiers: [],
               shippingOptions: [],
               total: {
                 label: 'Subscription',
                 amount: {
                   value: '1.00',
                   currency: 'USD',
                 },
               },
             })
      .canMakePayment();
}
