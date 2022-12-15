/*
 * Copyright 2019 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Installs KylePay payment app.
 * @param {String} method - The payment method name that this app supports.
 * @return {string} The 'success' or error message.
 */
async function install(method) {
  try {
    let registration = await navigator.serviceWorker.getRegistration('app.js');
    if (registration) {
      return 'The payment handler is already installed.';
    }
    await navigator.serviceWorker.register('app.js');
    await navigator.serviceWorker.ready;
    if (!registration.paymentManager) {
      return 'PaymentManager API not found.';
    }
    if (!registration.paymentManager.instruments) {
      return 'PaymentInstruments API not found.';
    }
    await registration.paymentManager.instruments.set('instrument-id', {
      name: 'Kyle Pay',
      method,
    });
    return enableDelegations();
  } catch (e) {
    return e.toString();
  }
}

/**
 * Enables the delegations for this payment method.
 * @return {Promise<string>} - Either "success" or an error message.
 */
async function enableDelegations() {
  try {
    let registration = await navigator.serviceWorker.getRegistration('app.js');
    await navigator.serviceWorker.ready;
    if (!registration.paymentManager) {
      return 'PaymentManager API not found.';
    }
    if (!registration.paymentManager.enableDelegations) {
      return 'PaymentManager does not support enableDelegations method';
    }
    await registration.paymentManager.enableDelegations(
        ['shippingAddress', 'payerName', 'payerEmail', 'payerPhone']);
    return 'success';
  } catch (e) {
    return e.toString();
  }
}
