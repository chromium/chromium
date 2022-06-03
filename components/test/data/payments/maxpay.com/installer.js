/*
 * Copyright 2020 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

const methodName = window.location.origin + '/pay';
const swSrcUrl = './payment_handler_sw.js';
let resultPromise;

/**
 * Update the installation status in the widget called 'installationStatus'.
 */
async function updateStatusView() {
  const installationStatusViewId = 'installationStatus';
  const registration = await navigator.serviceWorker.getRegistration(swSrcUrl);
  if (registration) {
    document.getElementById(installationStatusViewId).innerText = 'installed';
  } else {
    document.getElementById(installationStatusViewId).innerText = 'uninstalled';
  }
}

/**
 * Insert a message to the widget called 'log'.
 * @param {string} text - the text that is intended to be inserted into the log.
 */
function updateLogView(text) {
  const messageElement = document.getElementById('log');
  messageElement.innerText = text + '\n' + messageElement.innerText;
}

/**
 * Installs the payment handler.
 * @return {string} - the message about the installation result.
 */
async function install() { // eslint-disable-line no-unused-vars
  try {
    let registration =
        await navigator.serviceWorker.getRegistration(swSrcUrl);
    if (registration) {
      return 'The payment handler is already installed.';
    }

    await navigator.serviceWorker.register(swSrcUrl);
    registration = await navigator.serviceWorker.ready;
    await updateStatusView();

    if (!registration.paymentManager) {
      return 'PaymentManager API not found.';
    }

    await registration.paymentManager.instruments.set('instrument-id', {
      name: 'Instrument Name',
      method: methodName,
    });
    return 'success';
  } catch (e) {
    return e.message;
  }
}

/**
 * Uninstall the payment handler.
 * @return {string} - the message about the uninstallation result.
 */
async function uninstall() { // eslint-disable-line no-unused-vars
  let registration = await navigator.serviceWorker.getRegistration(swSrcUrl);
  if (!registration) {
    return 'The Payment handler has not been installed yet.';
  }
  await registration.unregister();
  await updateStatusView();
  return 'Uninstall successfully.';
}

/**
 * Launches the payment handler and waits until its window is ready.
 * @param {string} url - open a specified url in payment handler window.
 * @return {Promise<string>} - the message about the launch result.
 */
function launchAndWaitUntilReady( // eslint-disable-line no-unused-vars
    url = './payment_handler_window.html') {
  let appReadyResolver;
  appReadyPromise = new Promise((r) => {
    appReadyResolver = r;
  });
  try {
    const request = new PaymentRequest(
      [{supportedMethods: methodName, data: {url}}],
        {total: {label: 'Total', amount: {currency: 'USD', value: '0.01'}}});
    request.onpaymentmethodchange = (event) => {
      appReadyResolver(event.methodDetails.status);
    };
    resultPromise = request.show();
    updateLogView('launched and waiting until app gets ready.');
  } catch (e) {
    appReadyResolver(e.message);
  }
  return appReadyPromise;
}

/**
 * Gets the result of the PaymentRequest.show() from launchAndWaitUntilReady().
 * Precondition: called only when launchAndWaitUntilReady() returns
 * 'app_is_ready'.
 * @return {Promise<string>} - the payment handler's response to the
 * 'paymentrequest' event.
 */
async function getResult() { // eslint-disable-line no-unused-vars
  try {
    const response = await resultPromise;
    const result = response.details.status;
    updateLogView(result);
    await response.complete(result);
    return result;
  } catch (e) {
    updateLogView(e.message);
    return e.message;
  }
}

updateStatusView();
