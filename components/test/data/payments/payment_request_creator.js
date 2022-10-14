/*
 * Copyright 2020 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Creates a PaymentRequest object for the given |method|. If the |method| is
 * URL-based, that triggers manifest downloads.
 * @param {string} method - The payment method identifier to use.
 */
function createPaymentRequest(method) {
  new PaymentRequest(
      [{supportedMethods: method}],
      {total: {label: 'TEST', amount: {currency: 'USD', value: '0.01'}}});
}
