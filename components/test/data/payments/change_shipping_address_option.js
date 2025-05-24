/*
 * Copyright 2019 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

const methodName = window.location.origin;
let request = undefined;

/** Delegates handling of shipping address to the installed payment handler. */
async function delegateShippingAddressToPaymentHandler() {
  const registration = await navigator.serviceWorker.ready;
  await registration.paymentManager.enableDelegations(['shippingAddress']);
}

/**
 * Shows the payment sheet and outputs the return value of
 * PaymentRequestEvent.changeShippingOption().
 * @param {PaymentRequest} request The PaymentRequest object for showing the
 *     payment sheet.
 * @return {Promise<String>} The return value of
 *     PaymentRequestEvent.changeShippingOption.
 */
function outputChangeShippingAddressOptionReturnValue(request) {
  return request.show()
      .then((response) => {
        return response.complete('success').then(() => {
          return output(
              'PaymentRequest.show()',
              'changeShipping[Address|Option]() returned: ' +
                  JSON.stringify(response.details.changeShippingReturnedValue));
        });
      })
      .catch((error) => {
        return output('PaymentRequest.show() rejected with', error);
      });
}

/**
 * Creates a payment request with shipping requested.
 */
function createPaymentRequest() {
  request = new PaymentRequest(
      [{supportedMethods: methodName}], {
        total: {label: 'Total', amount: {currency: 'USD', value: '0.01'}},
        shippingOptions: [{
          label: 'Shipping option',
          id: 'id',
          amount: {currency: 'JPY', value: '0.05'},
          selected: false,
        }],
      },
      {requestShipping: true});
}

/**
 * @return {PaymentRequest} The Payment Request object for testNoHandler().
 */
function initTestNoHandler() {
  createPaymentRequest();
  return request;
}

/**
 * @param {String} eventType The type of the event to listen for.
 * @return {PaymentRequest} The Payment Request object for testReject().
 */
function initTestReject(eventType) {
  createPaymentRequest();
  request.addEventListener(eventType, (event) => {
    event.updateWith(Promise.reject('Error for test'));
  });
  return request;
}

/**
 * @param {String} eventType The type of the event to listen for.
 * @return {PaymentRequest} The Payment Request object for testThrow().
 */
function initTestThrow(eventType) {
  createPaymentRequest();
  request.addEventListener(eventType, (event) => {
    event.updateWith(new Promise(() => {
      throw new Error('Error for test');
    }));
  });
  return request;
}

/**
 * @param {String} eventType The type of the event to listen for.
 * @return {PaymentRequest} The Payment Request object for testDetails().
 */
function initTestDetails(eventType) {
  createPaymentRequest();
  request.addEventListener(eventType, (event) => {
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
          selected: true,
        },
      ],
      shippingAddressErrors: {
        country: 'US only shipping',
      },
    });
  });
  return request;
}
