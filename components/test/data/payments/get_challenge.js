/*
 * Copyright 2020 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

const kPaymentMethodIdentifier = 'secure-payment-confirmation';

/**
 * Returns the total amount from the clientDataJSON field of the given
 * PaymentResponse.
 * @param {PaymentResponse} response - A response from a secure payment
 * confirmation app.
 * @return {string} - Either the total amount or the string "undefined".
 */
function getTotalFromPaymentResponse(response) {
  try {
    if (!response || !response.details || !response.details.response ||
        !response.details.response.clientDataJSON) {
      return 'undefined';
    }
    const clientDataJSON = String.fromCharCode(...new Uint8Array(
        response.details.response.clientDataJSON));
    const clientData = JSON.parse(clientDataJSON);
    if (!clientData || !clientData.payment || !clientData.payment.total) {
      return 'undefined';
    }
    return clientData.payment.total.value;
  } catch (e) {
    return e.message;
  }
}

/**
 * Returns the total amount from the clientDataJSON field that was set by a
 * secure payment confirmation app.
 * @param {string} credentialId - The base64 encoded identifier of the
 * credential to use for payment.
 * @param {string} totalAmount - The total amount to be charged.
 * @return {Promise<string>} - Either the total amount or an error string.
 */
async function getTotalAmountFromClientData(credentialId, totalAmount) {
  try {
    const request = createPaymentRequest(credentialId, totalAmount, false, '');
    const response = await request.show();
    await response.complete();
    return getTotalFromPaymentResponse(response);
  } catch (e) {
    return e.message;
  }
}

/**
 * Returns the total amount from the clientDataJSON field that was set by a
 * secure payment confirmation app with a modified amount.
 * @param {string} credentialId - The base64 encoded identifier of the
 * credential to use for payment.
 * @param {string} modifiedTotal - The total amount to be charged in the
 * modifier.
 * @return {Promise<string>} - Either the total amount or an error string.
 */
async function getTotalAmountFromClientDataWithModifier(
    credentialId, modifiedTotal) {
  try {
    const request = createPaymentRequest(
        credentialId, '0', true, modifiedTotal);
    const response = await request.show();
    await response.complete();
    return getTotalFromPaymentResponse(response);
  } catch (e) {
    return e.message;
  }
}

/**
 * Returns the total amount from the clientDataJSON field that was set by a
 * secure payment confirmation app. Passes a promise into PaymentRequest.show()
 * that resolves with the finalized price after 0.5 seconds.
 * @param {string} credentialId - The base64 encoded identifier of the
 * credential to use for payment.
 * @param {string} finalizedTotal - The finalized amount to be charged.
 * @return {Promise<string>} - Either the total amount or an error string.
 */
async function getTotalAmountFromClientDataWithShowPromise(
    credentialId, finalizedTotal) {
  try {
    const request = createPaymentRequest(
        credentialId, '0', false, '');
    const response = await request.show(new Promise((resolve) => {
      window.setTimeout(() => {
        resolve(createDetails(finalizedTotal, false, ''));
      }, 500); // 0.5 seconds.
    }));
    await response.complete();
    return getTotalFromPaymentResponse(response);
  } catch (e) {
    return e.message;
  }
}

/**
 * Returns the total amount from the clientDataJSON field that was set by the
 * secure payment confirmation app. Passes a promise into PaymentRequest.show()
 * that resolves with the finalized and modified price after 0.5 seconds.
 * @param {string} credentialId - The base64 encoded identifier of the
 * credential to use for payment.
 * @param {string} finalizedModifierAmount - The finalized amount to be charged.
 * @return {Promise<string>} - Either the total amount or an error string.
 */
async function getTotalAmountFromClientDataWithModifierAndShowPromise(
    credentialId, finalizedModifierAmount) {
  try {
    const request = createPaymentRequest(credentialId, '0', false, '');
    const response = await request.show(new Promise((resolve) => {
      window.setTimeout(() => {
        resolve(createDetails('0', true, finalizedModifierAmount));
      }, 500); // 0.5 seconds.
    }));
    await response.complete();
    return getTotalFromPaymentResponse(response);
  } catch (e) {
    return e.message;
  }
}

/**
 * Creates a PaymentRequest object for secure payment confirmation method.
 * @param {string} credentialId - The base64 encoded identifier of the
 * credential to use for payment.
 * @param {string} totalAmount - The total amount to be charged.
 * @param {bool} withModifier - Whether modifier should be added.
 * @param {string} modifierAmount - The modifier amount, optional.
 * @return {PaymentRequest} - A PaymentRequest object.
 */
function createPaymentRequest(
    credentialId, totalAmount, withModifier, modifierAmount) {
  const challenge = new TextEncoder().encode('hello world');
  return new PaymentRequest(
      [{
        supportedMethods: kPaymentMethodIdentifier,
        data: {
          action: 'authenticate',
          credentialIds:
              [Uint8Array.from(atob(credentialId), (b) => b.charCodeAt(0))],
          timeout: 6000,
          payeeOrigin: 'https://example-payee-origin.test',
          challenge,
          instrument: {
            icon: window.location.origin + '/icon.png',
            displayName: 'My card',
          },
          rpId: 'a.com',
        },
      }],
      createDetails(totalAmount, withModifier, modifierAmount));
}

/**
 * Creates the payment details to be the second parameter of PaymentRequest
 * constructor.
 * @param {string} totalAmount - The total amount.
 * @param {bool} withModifier - Whether modifier should be added.
 * @param {string} modifierAmount - The modifier amount, optional.
 * @return {PaymentDetails} - The payment details with the given total amount.
 */
function createDetails(totalAmount, withModifier, modifierAmount) {
  const result = {
    total: {label: 'TEST', amount: {currency: 'USD', value: totalAmount}},
  };
  if (withModifier) {
    result.modifiers = [{
      supportedMethods: kPaymentMethodIdentifier,
      total: {
        label: 'MODIFIER TEST',
        amount: {currency: 'USD', value: modifierAmount}},
    }];
  }
  return result;
}
