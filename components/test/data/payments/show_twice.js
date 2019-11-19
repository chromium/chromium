/*
 * Copyright 2016 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Launches the PaymentRequest UI twice.
 */
function buy() { // eslint-disable-line no-unused-vars
  var payment1 = new PaymentRequest(
      [{supportedMethods: 'basic-card', data: {supportedNetworks: ['visa']}}],
      {total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}}});
  var payment2 = new PaymentRequest(
      [{supportedMethods: 'basic-card', data: {supportedNetworks: ['visa']}}],
      {total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}}});
  payment1.show();
  payment2.show().catch(function(error) {
    print('Second request: ' + error);
  });
}
