/*
 * Copyright 2020 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

const methodName = window.location.origin + '/pay';
const swSrcUrl = './payment_handler_sw.js';
let resultPromise;

/**
 * Insert a message to the widget called 'log'.
 * @param {string} text - the text that is intended to be inserted into the log.
 */
function updateLogView(text) {
  const messageElement = document.getElementById('log');
  messageElement.innerText = text + '\n' + messageElement.innerText;
}

/**
 * Launches the payment handler and waits until its window is ready.
 * @param {string} url - The URL to open in the payment handler window.
 * @param {string} paymentMethod - The payment method identifier.
 * @return {Promise<string>} - the message about the launch result.
 */
function launchAndWaitUntilReady(
    url = './payment_handler_window.html', paymentMethod = methodName) {
  let appReadyResolver;
  appReadyPromise = new Promise((r) => {
    appReadyResolver = r;
  });
  try {
    const request = new PaymentRequest(
      [{supportedMethods: paymentMethod, data: {url}}],
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
async function getResult() {
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
