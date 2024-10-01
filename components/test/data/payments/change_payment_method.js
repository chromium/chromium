/*
 * Copyright 2019 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

const methodName = window.location.origin;
let request = undefined;

/**
 * Shows the payment sheet and outputs the return value of
 * PaymentRequestEvent.changePaymentMethod().
 * @param {PaymentRequest} request - The PaymentRequest object for showing the
 *                                   payment sheet.
 * @return {String} The output.
 */
function outputChangePaymentMethodReturnValue(request) {
  return request.show()
      .then((response) => {
        return response.complete('success').then(() => {
          return output(
              'PaymentRequest.show()',
              'changePaymentMethod() returned: ' +
                  JSON.stringify(response.details.changePaymentMethodReturned),
          );
        });
      })
      .catch((error) => {
        return output('PaymentRequest.show() rejected with', error);
      });
}

/** @return {PaymentRequest} The Payment Request object for testNoHandler(). */
function initTestNoHandler() {
  request = new PaymentRequest([{supportedMethods: methodName}], {
    total: {label: 'Total', amount: {currency: 'USD', value: '0.01'}},
  });
  return request;
}

/**
 * Verifies that PaymentRequestEvent.changePaymentMethod() returns null if there
 * is no handler for the "paymentmethodchange" event in PaymentRequest.
 */
function testNoHandler() {
  // Intentionally do not respond to the 'paymentmethodchange' event.
  outputChangePaymentMethodReturnValue(initTestNoHandler());
}

/** @return {PaymentRequest} The Payment Request object for testReject(). */
function initTestReject() {
  request = new PaymentRequest([{supportedMethods: methodName}], {
    total: {label: 'Total', amount: {currency: 'USD', value: '0.01'}},
  });
  request.addEventListener('paymentmethodchange', (event) => {
    event.updateWith(Promise.reject('Error for test'));
  });
  return request;
}

/**
 * Verifies that PaymentRequest.show() is rejected if the promise passed into
 * PaymentMethodChangeEvent.updateWith() is rejected.
 */
function testReject() {
  outputChangePaymentMethodReturnValue(initTestReject());
}

/** @return {PaymentRequest} The Payment Request object for testThrow(). */
function initTestThrow() {
  request = new PaymentRequest([{supportedMethods: methodName}], {
    total: {label: 'Total', amount: {currency: 'USD', value: '0.01'}},
  });
  request.addEventListener('paymentmethodchange', (event) => {
    event.updateWith(
        new Promise(() => {
          throw new Error('Error for test');
        }),
    );
  });
  return request;
}

/**
 * Verifies that PaymentRequest.show() is rejected if there is an exception in
 * the promised passed into PaymentMethodChangeEvent.updateWith().
 */
function testThrow() {
  outputChangePaymentMethodReturnValue(initTestThrow());
}

/** @return {PaymentRequest} The Payment Request object for testDetails(). */
function initTestDetails() {
  request = new PaymentRequest([{supportedMethods: methodName}], {
    total: {label: 'Total', amount: {currency: 'USD', value: '0.01'}},
  });
  request.addEventListener('paymentmethodchange', (event) => {
    event.updateWith({
      total: {label: 'Total', amount: {currency: 'GBP', value: '0.02'}},
      error: 'Error for test',
      modifiers: [
        {
          supportedMethods: methodName,
          data: {soup: 'potato'},
          total: {
            label: 'Modified total',
            amount: {currency: 'EUR', value: '0.03'},
          },
          additionalDisplayItems: [
            {
              label: 'Modified display item',
              amount: {currency: 'INR', value: '0.06'},
            },
          ],
        },
        {
          supportedMethods: methodName + '/other',
          data: {soup: 'tomato'},
          total: {
            label: 'Modified total #2',
            amount: {currency: 'CHF', value: '0.07'},
          },
          additionalDisplayItems: [
            {
              label: 'Modified display item #2',
              amount: {currency: 'CAD', value: '0.08'},
            },
          ],
        },
      ],
      paymentMethodErrors: {country: 'Unsupported country'},
      displayItems: [
        {
          label: 'Display item',
          amount: {currency: 'CNY', value: '0.04'},
        },
      ],
      shippingOptions: [
        {
          label: 'Shipping option',
          id: 'id',
          amount: {currency: 'JPY', value: '0.05'},
        },
      ],
    });
  });
  return request;
}

/**
 * Verifies that PaymentRequestEvent.changePaymentMethod() returns a subset of
 * details passed into PaymentMethodChangeEvent.updateWith() method.
 */
function testDetails() {
  outputChangePaymentMethodReturnValue(initTestDetails());
}
