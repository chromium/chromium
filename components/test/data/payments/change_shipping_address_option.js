/*
 * Copyright 2019 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

let methodName = window.location.origin + '/pay';
let request = undefined;

/**
 * Installs the payment handler.
 * @param {String} swApp The name of the service worker based payment app to
 *     install.
 */
function install(swApp) { // eslint-disable-line no-unused-vars
  navigator.serviceWorker.getRegistration(swApp)
      .then((registration) => {
        if (registration) {
          output(
              'serviceWorker.getRegistration()',
              'The ServiceWorker is already installed.');
          return;
        }
        navigator.serviceWorker.register(swApp)
            .then(() => {
              return navigator.serviceWorker.ready;
            })
            .then((registration) => {
              if (!registration.paymentManager) {
                output(
                    'serviceWorker.register()',
                    'PaymentManager API not found.');
                return;
              }

              registration.paymentManager.enableDelegations(['shippingAddress'])
                  .then(() => {
                    registration.paymentManager.instruments
                        .set('instrument-id', {
                          name: 'Instrument Name',
                          method: methodName,
                        })
                        .then(() => {
                          output(
                              'instruments.set()',
                              'Payment handler installed.');
                        })
                        .catch((error) => {
                          output('instruments.set() rejected with', error);
                        });
                  })
                  .catch((error) => {
                    output('enableDelegations() rejected with', error);
                  });
            })
            .catch((error) => {
              output('serviceWorker.register() rejected with', error);
            });
      })
      .catch((error) => {
        output('serviceWorker.getRegistration() rejected with', error);
      });
}

/**
 * Shows the payment sheet and outputs the return value of
 * PaymentRequestEvent.changeShippingOption().
 * @param {PaymentRequest} request The PaymentRequest object for showing the
 *     payment sheet.
 */
function outputChangeShippingAddressOptionReturnValue(request) { // eslint-disable-line no-unused-vars, max-len
  request.show()
      .then((response) => {
        response.complete('success').then(() => {
          output(
              'PaymentRequest.show()',
              'changeShipping[Address|Option]() returned: ' +
                  JSON.stringify(response.details.changeShippingReturnedValue));
        });
      })
      .catch((error) => {
        output('PaymentRequest.show() rejected with', error);
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
function initTestNoHandler() { // eslint-disable-line no-unused-vars
  createPaymentRequest();
  return request;
}

/**
 * @param {String} eventType The type of the event to listen for.
 * @return {PaymentRequest} The Payment Request object for testReject().
 */
function initTestReject(eventType) { // eslint-disable-line no-unused-vars
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
function initTestThrow(eventType) { // eslint-disable-line no-unused-vars
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
function initTestDetails(eventType) { // eslint-disable-line no-unused-vars
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
          supportedMethods: methodName + '2',
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
