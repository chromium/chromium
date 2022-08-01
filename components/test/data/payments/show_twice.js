/*
 * Copyright 2016 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Launches the first PaymentRequest UI.
 */
function showFirst() { // eslint-disable-line no-unused-vars
  const request1 = new PaymentRequest(
      [{supportedMethods: 'https://bobpay.com'}, {supportedMethods: 'https://alicepay.com'}],
      {total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}}});
  request1.show();
}

/**
 * Launches the second PaymentRequest UI, which should fail because the first is
 * already showing. Must be called after showFirst().
 */
async function showSecond() { // eslint-disable-line no-unused-vars
  const request2 = new PaymentRequest(
      [{supportedMethods: 'https://bobpay.com'}],
      {total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}}});
  // We already have a PaymentRequest showing, so this should fail.
  try {
    await request2.show();
    return 'Unexpected success showing second request';
  } catch (error) {
    return 'Second request: ' + error;
  }
}
