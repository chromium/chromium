/*
 * Copyright 2020 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Requests a secure payment confirmation payment for the given credential
 * identifier and payment method data.
 * @param {string} credentialId - The base64 encoded identifier of the
 * credential to use for payment.
 * @param {object} data - the data to be passed in PaymentMethodData.
 * @return {Promise<string>} - Either the clientDataJSON string or an error
 * message.
 */
async function requestPaymentWithData(credentialId, data) {
  try {
    const request = new PaymentRequest(
      [{
        supportedMethods: 'secure-payment-confirmation',
        data: data,
      }],
      {total: {label: 'TEST', amount: {currency: 'USD', value: '0.01'}}});
    const response = await request.show();
    await response.complete();
    return String.fromCharCode(...new Uint8Array(
      response.details.response.clientDataJSON));
  } catch (e) {
    return e.message;
  }
}

/**
 * Requests a secure payment confirmation payment for the given credential
 * identifier.
 * @param {string} credentialId - The base64 encoded identifier of the
 * credential to use for payment.
 * @return {Promise<string>} - Either the clientDataJSON string or an error
 * message.
 */
async function requestPayment(credentialId) {
  return requestPaymentWithData(credentialId, {
    action: 'authenticate',
    credentialIds:
      [Uint8Array.from(atob(credentialId), (b) => b.charCodeAt(0))],
    challenge: new TextEncoder().encode('hello world'),
    instrument: {
      displayName: 'Hello World',
      icon: window.location.origin + '/icon.png',
    },
    timeout: 6000,
    payeeOrigin: 'https://example-payee-origin.test',
    rpId: 'a.com',
  });
}

/**
 * Requests a secure payment confirmation payment for the given credential
 * identifier with payeeName data instead of payeeOrigin.
 * @param {string} credentialId - The base64 encoded identifier of the
 * credential to use for payment.
 * @return {Promise<string>} - Either the clientDataJSON string or an error
 * message.
 */
async function requestPaymentWithPayeeName(credentialId) {
  return requestPaymentWithData(credentialId, {
    action: 'authenticate',
    credentialIds:
      [Uint8Array.from(atob(credentialId), (b) => b.charCodeAt(0))],
    challenge: new TextEncoder().encode('hello world'),
    instrument: {
      displayName: 'Hello World',
      icon: window.location.origin + '/icon.png',
    },
    timeout: 6000,
    payeeName: 'Example Payee',
    rpId: 'a.com',
  });
}

/**
 * Requests a secure payment confirmation payment for the given credential
 * identifier with both payeeName and payeeOrigin data.
 * @param {string} credentialId - The base64 encoded identifier of the
 * credential to use for payment.
 * @return {Promise<string>} - Either the clientDataJSON string or an error
 * message.
 */
async function requestPaymentWithPayeeNameAndOrigin(credentialId) {
  return requestPaymentWithData(credentialId, {
    action: 'authenticate',
    credentialIds:
      [Uint8Array.from(atob(credentialId), (b) => b.charCodeAt(0))],
    challenge: new TextEncoder().encode('hello world'),
    instrument: {
      displayName: 'Hello World',
      icon: window.location.origin + '/icon.png',
    },
    timeout: 6000,
    payeeName: 'Example Payee',
    payeeOrigin: 'https://example-payee-origin.test',
    rpId: 'a.com',
  });
}
