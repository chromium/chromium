/*
 * Copyright 2020 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

window.onmessage = (e) => {
  requestPayment(e.data).then((result) => {
    e.source.postMessage(result, e.origin);
  }).catch((error) => {
    e.source.postMessage(error, e.origin);
  });
};

/**
 * Requests a secure payment confirmation payment for the given credential
 * identifier.
 * @param {string} credentialId - The base64 encoded identifier of the
 * credential to use for payment.
 * @return {Promise<object>} - Either the string 'success' or an error message.
 */
async function requestPayment(credentialId) {
  try {
    const request = new PaymentRequest(
        [{supportedMethods: 'secure-payment-confirmation',
          data: {
            action: 'authenticate',
            credentialIds: [Uint8Array.from(atob(credentialId),
                                            (b) => b.charCodeAt(0))],
            networkData: new TextEncoder().encode('hello world'),
            timeout: 6000,
            fallbackUrl: 'https://fallback.example/url',
          }}],
        {total: {label: 'TEST', amount: {currency: 'USD', value: '0.01'}}});
    const response = await request.show();
    await response.complete();
    return 'success';
  } catch (e) {
    return e.message;
  }
}
