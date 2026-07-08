/*
 * Copyright 2019 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

const DEFAULT_METHOD_NAME = window.location.origin;
const SW_SRC_URL = 'payment_handler_sw.js';

const methodName = DEFAULT_METHOD_NAME;
let request;

/**
 * Uninstalls the payment handler.
 * @param {string} swSrcUrlOverride - Optional service worker JavaScript file
 * URL.
 * @return {Promise<string>} - 'success' or error message on failure.
 */
async function uninstall(swSrcUrlOverride) {
  const swSrcUrl =
      (swSrcUrlOverride !== undefined) ? swSrcUrlOverride : SW_SRC_URL;
  try {
    const registration =
        await navigator.serviceWorker.getRegistration(swSrcUrl);
    if (!registration) {
      return 'The Payment handler has not been installed yet.';
    }
    await registration.unregister();
    return 'success';
  } catch (e) {
    return e.toString();
  }
}

/**
 * Delegates handling of the provided options to the payment handler.
 * @param {Array<string>} delegations The list of payment options to delegate.
 * @return {Promise<string>} - 'success' or error message on failure.
 */
async function enableDelegations(delegations) {
  try {
    await navigator.serviceWorker.ready;
    const registration =
        await navigator.serviceWorker.getRegistration(SW_SRC_URL);
    if (!registration) {
      return 'The payment handler is not installed.';
    }
    if (!registration.paymentManager) {
      return 'PaymentManager API not found.';
    }
    if (!registration.paymentManager.enableDelegations) {
      return 'PaymentManager does not support enableDelegations method';
    }

    await registration.paymentManager.enableDelegations(delegations);
    return 'success';
  } catch (e) {
    return e.toString();
  }
}

/**
 * Launches the payment handler.
 * @param {string} methodNameOverride - Optional payment method identifier.
 * @return {Promise<string>} - 'success' or error message on failure.
 */
async function launch(methodNameOverride) {
  const method =
      (methodNameOverride !== undefined) ? methodNameOverride : methodName;
  try {
    const request = new PaymentRequest([{supportedMethods: method}], {
      total: {label: 'Total', amount: {currency: 'USD', value: '0.01'}},
    });
    const response = await request.show();
    await response.complete('success');
    return 'success';
  } catch (e) {
    return e.toString();
  }
}

let responsePromise = null;

/**
 * Launches the payment handler without waiting for a response to be returned.
 * @param {string} methodNameOverride - The payment method to launch. If not
 *     specified, the global methodName set from install() will be used.
 * @param {string} windowPage - The page to load in the payment handler window.
 * @return {string} The 'success' or error message.
 */
function launchWithoutWaitForResponse(methodNameOverride, windowPage) {
  const method =
      (methodNameOverride !== undefined) ? methodNameOverride : methodName;
  return launchWithoutWaitForResponseWithMethods(
      [{supportedMethods: method, data: {'windowPage': windowPage}}]);
}

/**
 * Launches the payment handler without waiting for a response to be returned.
 * @param {sequence<PaymentMethodData>} methodData An array of payment method
 *        objects.
 * @return {string} The 'success' or error message.
 */
function launchWithoutWaitForResponseWithMethods(methodData) {
  try {
    request = new PaymentRequest(methodData, {
      total: {label: 'Total', amount: {currency: 'USD', value: '0.01'}},
    });
    responsePromise = request.show();
    return 'success';
  } catch (e) {
    return e.toString();
  }
}

/**
 * Launches the payment handler and wait until the window client and service
 * worker are ready.
 * @param {string} methodNameOverride - Optional payment method identifier.
 * @return {Promise<string>} - 'success' or error message on failure.
 */
async function launchAndWaitUntilAppReady(methodNameOverride) {
  let appReadyResolver;
  appReadyPromise = new Promise((r) => {
    appReadyResolver = r;
  });
  const method =
      (methodNameOverride !== undefined) ? methodNameOverride : methodName;
  try {
    request = new PaymentRequest([{supportedMethods: method}], {
      total: {label: 'Total', amount: {currency: 'USD', value: '0.01'}},
    });
    request.show();
    request.onpaymentmethodchange = (event) => {
      // The payment app service worker calls changePaymentMethod() when its
      // window client is ready. We leverage this event as a signaling channel
      // to notify the test that the payment app is ready.
      //
      // Calling updateWith() is required by the Payment Request when
      // handling paymentmethodchange. We don't care what's updated here.
      event.updateWith({});
      // Acknowledge the payment app readiness and resolve the promise.
      if (event.methodDetails.status === 'success') {
        appReadyResolver(event.methodDetails.status);
      }
    };
    return appReadyPromise;
  } catch (e) {
    return appReadyResolver(e.toString());
  }
}

/**
 * Completes the resolved payment response with the specified status.
 * @param {string} status - 'success' or 'fail'.
 * @return {Promise<string>} - 'success' or error message on failure.
 */
async function completeResponse(status) {
  try {
    const response = await responsePromise;
    await response.complete(status);
    return 'success';
  } catch (e) {
    return e.toString();
  }
}

/**
 * Launches the payment handler and registers a .complete('success') handler
 * asynchronously on the promise resolved by show(). Returns 'success'
 * synchronously.
 * @param {string} methodNameOverride - Optional payment method identifier.
 * @return {string} - 'success' or error message on failure.
 */
function launchAndComplete(methodNameOverride) {
  const method =
      (methodNameOverride !== undefined) ? methodNameOverride : methodName;
  try {
    request = new PaymentRequest([{supportedMethods: method}], {
      total: {label: 'Total', amount: {currency: 'USD', value: '0.01'}},
    });
    request.show().then(r => r.complete('success'));
    return 'success';
  } catch (e) {
    return e.toString();
  }
}

/**
 * Aborts the on-going payment request.
 * @return {Promise<string>} - 'success' or error message on failure.
 */
async function abort() {
  try {
    await request.abort();
    return 'success';
  } catch (e) {
    return e.toString();
  }
}

let paymentOptions = null;

/**
 * Creates a payment request with required information and calls request.show()
 * to invoke payment sheet UI. To ensure that UI gets shown two payment methods
 * are supported: One URL-based and one 'basic-card'.
 * @param {Object} options - The list of requested paymentOptions.
 * @param {string} paymentMethod - A URL-based payment method identifier.
 * @return {Promise<string>} - The 'success' or error message.
 */
async function paymentRequestWithOptions(options, paymentMethod) {
  paymentOptions = options;
  if (!paymentMethod) {
    return 'Payment method required';
  }
  try {
    const request = new PaymentRequest([{supportedMethods: paymentMethod}],
      {
        total: {
          label: 'Total',
          amount: {
            currency: 'USD',
            value: '0.01',
          },
        },
        shippingOptions: [{
          id: 'freeShippingOption',
          label: 'Free global shipping',
          amount: {
            currency: 'USD',
            value: '0',
          },
          selected: true,
        }],
      },
      options);

    const response = await request.show();
    return validatePaymentResponse(response);
  } catch (e) {
    return e.toString();
  }
}

/**
 * Validates the response received from payment handler.
 * @param {Object} response - The response received from payment handler.
 * @param {Promise<string>} - Either 'success' or an error message.
 */
async function validatePaymentResponse(response) {
  try {
    let isValid = true;
    if (paymentOptions.requestShipping) {
      isValid = ('freeShippingOption' === response.shippingOption) &&
          ('Reston' === response.shippingAddress.city) &&
          ('US' === response.shippingAddress.country) &&
          ('20190' === response.shippingAddress.postalCode) &&
          ('VA' === response.shippingAddress.region);
    }

    isValid = isValid &&
        (!paymentOptions.requestPayerName ||
        ('John Smith' === response.payerName)) &&
        (!paymentOptions.requestPayerEmail ||
        ('smith@gmail.com' === response.payerEmail)) &&
        (!paymentOptions.requestPayerPhone ||
        ('+15555555555' === response.payerPhone));

    await response.complete(isValid ? 'success' : 'fail');
    return 'success';
  } catch (e) {
    return e.toString();
  }
}
