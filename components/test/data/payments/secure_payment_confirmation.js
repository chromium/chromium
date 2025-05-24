/*
 * Copyright 2020 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// Most test callers immediately wait on the response from
// |getStatusForMethodData|. However some tests trigger the UI and then interact
// with it, and to support those we save the promise for later retrieval.
let statusPromise = null;

/**
 * Return the outstanding status promise, if any.
 * @return {string} - The status field or error message.
 */
async function getOutstandingStatusPromise() {
  return statusPromise;
}

/**
 * Creates and returns the first parameter to the PaymentRequest constructor for
 * secure payment confirmation.
 * @param {string} credentialIdentifier - An optional base64 encoded credential
 * identifier. If not specified, then 'cred' is used instead.
 * @param {string} iconUrl - An optional icon URL. If not specified, then
 * 'window.location.origin/icon.png' is used.
 * @param {boolean} showOptOut - Whether to show the SPC opt-out experience.
 * If not specified, the parameter is not set in the input data blob.
 * @return {Array<PaymentMethodData>} - Secure payment confirmation method data.
 */
function getTestMethodData(credentialIdentifier, iconUrl, showOptOut) {
  return getTestMethodDataWithInstrument(
    {
      displayName: 'display_name_for_instrument',
      icon: iconUrl ? iconUrl : window.location.origin + '/icon.png',
    },
    credentialIdentifier,
    showOptOut);
}

/**
 * Creates and returns the first parameter to the PaymentRequest constructor for
 * secure payment confirmation.
 * @param {PaymentInstrument} paymentInstrument - Payment instrument details to
 * be included in the request.
 * @param {string} credentialIdentifier - An optional base64 encoded credential
 * identifier. If not specified, then 'cred' is used instead.
 * @param {boolean} showOptOut - Whether to show the SPC opt-out experience.
 * If not specified, the parameter is not set in the input data blob.
 * @return {Array<PaymentMethodData>} - Secure payment confirmation method data.
 */
function getTestMethodDataWithInstrument(
  paymentInstrument, credentialIdentifier, showOptOut) {
  const methodData = {
    supportedMethods: 'secure-payment-confirmation',
    data: {
      action: 'authenticate',
      credentialIds: [Uint8Array.from(
          (credentialIdentifier ? atob(credentialIdentifier) : 'cred'),
          (c) => c.charCodeAt(0))],
      challenge: Uint8Array.from('challenge', (c) => c.charCodeAt(0)),
      instrument: paymentInstrument,
      timeout: 60000,
      payeeOrigin: 'https://example-payee-origin.test',
      rpId: 'a.com',
    },
  };

  if (typeof showOptOut !== 'undefined') {
    methodData.data.showOptOut = showOptOut;
  }

  return [methodData];
}

/**
 * Returns the status field of the response to a secure payment confirmation
 * request.
 * @param {string} credentialIdentifier - An optional base64 encoded credential
 * identifier. If not specified, then 'cred' is used instead.
 * @param {string} iconUrl - An optional icon URL. If not specified, then
 * 'window.location.origin/icon.png' is used.
 * @param {boolean} showOptOut - Whether to show the SPC opt-out experience.
 * If not specified, the parameter is not set in the input data blob.
 * @return {string} - The status field or error message.
 */
async function getSecurePaymentConfirmationStatus(
    credentialIdentifier, iconUrl, showOptOut) {
  statusPromise = getStatusForMethodData(
      getTestMethodData(credentialIdentifier, iconUrl, showOptOut));
  return statusPromise;
}

/**
 * Checks the result of canMakePayment() (ignoring its actual result) and then
 * returns the status field of the response to a secure payment confirmation
 * request.
 * @return {string} - The status field or error message.
 */
async function getSecurePaymentConfirmationStatusAfterCanMakePayment() {
  statusPromise = getStatusForMethodDataAfterCanMakePayment(
      getTestMethodData(), /* checkCanMakePaymentFirst = */true);
  return statusPromise;
}

/**
 * Returns the clientDataJSON's payment.instrument.icon value of the response to
 * a secure payment confirmation request.
 * @param {PaymentInstrument} paymentInstrument - Payment instrument details to
 * be included in the request.
 * @param {string} credentialIdentifier - base64 encoded credential identifier.
 * @return {string} - Output instrument icon string.
 */
async function getSecurePaymentConfirmationResponseIconWithInstrument(
    paymentInstrument, credentialIdentifier) {
  const methodData = getTestMethodDataWithInstrument(
    paymentInstrument, credentialIdentifier);
  const request = new PaymentRequest(
    methodData,
    {total: {label: 'TEST', amount: {currency: 'USD', value: '0.01'}}});

  const response = await request.show();
  await response.complete();
  const clientData = JSON.parse(String.fromCharCode(...new Uint8Array(
    response.details.response.clientDataJSON)));
  return clientData.payment.instrument.icon;
}

/**
 * Checks whether secure payment confirmation can make payments.
 * @param {string} iconUrl - An optional icon URL. If not specified, then
 * 'window.location.origin/icon.png' is used.
 * @return {string} - 'true', 'false', or error message on failure.
 */
async function securePaymentConfirmationCanMakePayment(iconUrl) {
  return canMakePaymentForMethodData(getTestMethodData(
      /* credentialIdentifier = */undefined, iconUrl));
}

/**
 * Creates a PaymentRequest for secure payment confirmation, checks
 * canMakePayment twice, and returns the second value.
 * @return {string} - 'true', 'false', or error message on failure.
 */
async function securePaymentConfirmationCanMakePaymentTwice() {
  return canMakePaymentForMethodDataTwice(getTestMethodData());
}

/**
 * Checks whether secure payment confirmation has enrolled instruments.
 * @return {string} - 'true', 'false', or error message on failure.
 */
async function securePaymentConfirmationHasEnrolledInstrument() {
  return hasEnrolledInstrumentForMethodData(getTestMethodData());
}

/**
 * Creates a secure payment confirmation credential, returning a JSON
 * blob of information about the created credential.
 *
 * @param {string} userId - The user.id for the created credential.
 * @return {Promise<object>} - Either information about the created credential
 *     or an error message.
 */
async function createPaymentCredential(userId) {
  const textEncoder = new TextEncoder();
  const publicKeyRP = {
      id: 'a.com',
      name: 'Acme',
  };
  const publicKeyParameters = [{
      type: 'public-key',
      alg: -7,
  }];
  const publicKey = {
    user: {
      displayName: 'User',
      id: textEncoder.encode(userId),
      name: 'user@acme.com',
    },
    rp: publicKeyRP,
    challenge: textEncoder.encode('climb a mountain'),
    pubKeyCredParams: publicKeyParameters,
    extensions: {payment: {isPayment: true}},
  };

  try {
    const credential = await navigator.credentials.create({publicKey});
    const webIdlType = credential.constructor.name;
    const type = JSON.parse(String.fromCharCode(...new Uint8Array(
                                credential.response.clientDataJSON)))
                     .type;
    const id = btoa(String.fromCharCode(...new Uint8Array(credential.rawId)));

    return JSON.stringify({webIdlType, type, id});
  } catch (e) {
    return e.toString();
  }
}
