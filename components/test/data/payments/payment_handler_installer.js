/*
 * Copyright 2020 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Installs the given payment handler with the given payment methods.
 * @param {string} swUrl - The relative URL of the service worker JavaScript
 * file to install.
 * @param {string[]} methods - The list of payment methods that this service
 * worker supports.
 * @param {bool} ownScopeMethod - Whether this service worker should support its
 * own scope as a payment method.
 * @return {Promise<string>} - 'success' or error message on failure.
 */
async function install(swUrl, methods, ownScopeMethod) { // eslint-disable-line no-unused-vars, max-len
  try {
    const registration = await navigator.serviceWorker.register(swUrl);
    await activation(registration);
    for (let method of methods) {
      await registration.paymentManager.instruments.set(
          'instrument-for-' + method, {name: 'Instrument Name', method});
    }
    if (ownScopeMethod) {
      await registration.paymentManager.instruments.set(
          'instrument-for-own-scope',
          {name: 'Instrument Name', method: registration.scope});
    }
    return 'success';
  } catch (e) {
    return e.message;
  }
}

/**
 * Returns a promise that resolves when the service worker of the given
 * registration has activated.
 * @param {ServiceWorkerRegistration} registration - A service worker
 * registration.
 * @return {Promise<void>} - A promise that resolves when the service worker
 * has activated.
 */
async function activation(registration) {
  return new Promise((resolve) => {
    if (registration.active) {
      resolve();
      return;
    }
    registration.addEventListener('updatefound', () => {
      const newWorker = registration.installing;
      if (newWorker.state == 'activated') {
        resolve();
        return;
      }
      newWorker.addEventListener('statechange', () => {
        if (newWorker.state == 'activated') {
          resolve();
        }
      });
    });
  });
}
