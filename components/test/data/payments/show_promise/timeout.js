/*
 * Copyright 2019 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Launch PaymentRequest with a show promise and don't resolve or reject it.
 * @return {string} - The error message, if any.
 */
async function buy() { // eslint-disable-line no-unused-vars
  try {
    await new PaymentRequest(
        [{supportedMethods: window.location.href}],
        {total: {label: 'Total', amount: {currency: 'USD', value: '1.00'}}})
        .show(new Promise(function(resolve) { /* Intentionally empty. */ }));
  } catch (error) {
    // Error is both printed and returned as the Java test reads it from the
    // page and the C++ browser test reads the return value.
    print(error);
    return error.toString();
  }
}
