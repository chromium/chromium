/*
 * Copyright 2019 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Launch PaymentRequest with a show promise and reject that promise.
 */
function buy() { // eslint-disable-line no-unused-vars
  try {
    new PaymentRequest(
        [{supportedMethods: 'basic-card'}],
        {total: {label: 'Total', amount: {currency: 'USD', value: '1.00'}}})
        .show(new Promise(function(resolve, reject) {
          reject();
        }))
        .catch(function(error) {
          print(error);
        });
  } catch (error) {
    print(error);
  }
}
