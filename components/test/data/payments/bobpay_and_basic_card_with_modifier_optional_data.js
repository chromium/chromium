/*
 * Copyright 2017 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Launches the PaymentRequest UI with Bob Pay and 'basic-card' as
 * payment methods, and Bob Pay modifier.
 */
function buy() { // eslint-disable-line no-unused-vars
  try {
    new PaymentRequest(
        [
          {supportedMethods: 'https://bobpay.com'},
          {supportedMethods: 'basic-card'},
        ],
        {
          total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}},
          modifiers: [{
            supportedMethods: 'https://bobpay.com',
            total: {
              label: 'Total',
              amount: {currency: 'USD', value: '4.00'},
            },
            additionalDisplayItems: [{
              label: 'BobPay discount',
              amount: {currency: 'USD', value: '-1.00'},
            }],
            data: {
              discountProgramParticipantId: '86328764873265',
            },
          }],
        })
        .show()
        .then(function(resp) {
          resp.complete('success')
              .then(function() {
                print(JSON.stringify(resp, undefined, 2));
              })
              .catch(function(error) {
                print('complete() rejected<br>' + error.message);
              });
        })
        .catch(function(error) {
          print('show() rejected<br>' + error.message);
        });
  } catch (error) {
    print('exception thrown<br>' + error.message);
  }
}

/**
 * Launches the PaymentRequest UI with 'basic-card' payment method and
 * all cards modifier.
 */
function buyWithAllCardsModifier() { // eslint-disable-line no-unused-vars
  try {
    new PaymentRequest([{supportedMethods: 'basic-card'}], {
      total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}},
      modifiers: [{
        supportedMethods: 'basic-card',
        total: {
          label: 'Total',
          amount: {currency: 'USD', value: '4.00'},
        },
        additionalDisplayItems: [{
          label: 'basic-card discount',
          amount: {currency: 'USD', value: '-1.00'},
        }],
        data: {discountProgramParticipantId: '86328764873265'},
      }],
    })
        .show()
        .then(function(resp) {
          resp.complete('success')
              .then(function() {
                print(JSON.stringify(resp, undefined, 2));
              })
              .catch(function(error) {
                print('complete() rejected<br>' + error.message);
              });
        })
        .catch(function(error) {
          print('show() rejected<br>' + error.message);
        });
  } catch (error) {
    print('exception thrown<br>' + error.message);
  }
}

/**
 * Launches the PaymentRequest UI with 'basic-card' as payment method and
 * visa credit card modifier.
 */
function buyWithVisaCreditModifier() { // eslint-disable-line no-unused-vars
  try {
    new PaymentRequest(
        [{supportedMethods: 'basic-card'}],
        {
          total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}},
          modifiers: [{
            supportedMethods: 'basic-card',
            total: {
              label: 'Total',
              amount: {currency: 'USD', value: '4.00'},
            },
            additionalDisplayItems: [{
              label: 'Visa credit discount',
              amount: {currency: 'USD', value: '-1.00'},
            }],
            data: {
              discountProgramParticipantId: '86328764873265',
              supportedTypes: ['credit'],
              supportedNetworks: ['visa'],
            },
          }],
        })
        .show()
        .then(function(resp) {
          resp.complete('success')
              .then(function() {
                print(JSON.stringify(resp, undefined, 2));
              })
              .catch(function(error) {
                print('complete() rejected<br>' + error.message);
              });
        })
        .catch(function(error) {
          print('show() rejected<br>' + error.message);
        });
  } catch (error) {
    print('exception thrown<br>' + error.message);
  }
}

/**
 * Launches the PaymentRequest UI with 'basic-card' as payment method and
 * visa debit card modifier.
 */
function buyWithVisaDebitModifier() { // eslint-disable-line no-unused-vars
  try {
    new PaymentRequest(
        [{
          supportedMethods: 'basic-card',
        }],
        {
          total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}},
          modifiers: [{
            supportedMethods: 'basic-card',
            total: {
              label: 'Total',
              amount: {currency: 'USD', value: '4.00'},
            },
            additionalDisplayItems: [{
              label: 'Visa debit discount',
              amount: {currency: 'USD', value: '-1.00'},
            }],
            data: {
              discountProgramParticipantId: '86328764873265',
              supportedTypes: ['debit'],
              supportedNetworks: ['visa'],
            },
          }],
        })
        .show()
        .then(function(resp) {
          resp.complete('success')
              .then(function() {
                print(JSON.stringify(resp, undefined, 2));
              })
              .catch(function(error) {
                print('complete() rejected<br>' + error.message);
              });
        })
        .catch(function(error) {
          print('show() rejected<br>' + error.message);
        });
  } catch (error) {
    print('exception thrown<br>' + error.message);
  }
}

/**
 * Launches the PaymentRequest UI with 'basic-card' as payment method and
 * credit card modifier.
 */
function buyWithCreditModifier() { // eslint-disable-line no-unused-vars
  try {
    new PaymentRequest(
        [{
          supportedMethods: 'basic-card',
        }],
        {
          total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}},
          modifiers: [{
            supportedMethods: 'basic-card',
            total: {
              label: 'Total',
              amount: {currency: 'USD', value: '4.00'},
            },
            additionalDisplayItems: [{
              label: 'Credit card discount',
              amount: {currency: 'USD', value: '-1.00'},
            }],
            data: {
              discountProgramParticipantId: '86328764873265',
              supportedTypes: ['credit'],
            },
          }],
        })
        .show()
        .then(function(resp) {
          resp.complete('success')
              .then(function() {
                print(JSON.stringify(resp, undefined, 2));
              })
              .catch(function(error) {
                print('complete() rejected<br>' + error.message);
              });
        })
        .catch(function(error) {
          print('show() rejected<br>' + error.message);
        });
  } catch (error) {
    print('exception thrown<br>' + error.message);
  }
}

/**
 * Launches the PaymentRequest UI with 'basic-card' as payment method and
 * visa card modifier.
 */
function buyWithVisaModifier() { // eslint-disable-line no-unused-vars
  try {
    new PaymentRequest(
        [{
          supportedMethods: 'basic-card',
        }],
        {
          total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}},
          modifiers: [{
            supportedMethods: 'basic-card',
            total: {
              label: 'Total',
              amount: {currency: 'USD', value: '4.00'},
            },
            additionalDisplayItems: [{
              label: 'Visa discount',
              amount: {currency: 'USD', value: '-1.00'},
            }],
            data: {
              discountProgramParticipantId: '86328764873265',
              supportedNetworks: ['visa'],
            },
          }],
        })
        .show()
        .then(function(resp) {
          resp.complete('success')
              .then(function() {
                print(JSON.stringify(resp, undefined, 2));
              })
              .catch(function(error) {
                print('complete() rejected<br>' + error.message);
              });
        })
        .catch(function(error) {
          print('show() rejected<br>' + error.message);
        });
  } catch (error) {
    print('exception thrown<br>' + error.message);
  }
}

/**
 * Creates a payment request with required information and calls request.show()
 * to launch PaymentRequest UI. To ensure that UI gets shown two payment methods
 * are supported: One url-based and one 'basic-card'.
 * @param {Object} options The list of requested paymentOptions.
 * @return {string} The 'success' or error message.
 */
function paymentRequestWithOptions(options) { // eslint-disable-line no-unused-vars, max-len
  try {
    const request = new PaymentRequest(
        [
          {supportedMethods: 'https://bobpay.xyz'},
          {supportedMethods: 'basic-card'},
        ],
        {
          total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}},
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
    request.show();
    return 'success';
  } catch (e) {
    return e.toString();
  }
}

/**
 * Launches the PaymentRequest UI with 'basic-card' and 'bobpay' as payment
 * methods and shipping address requested.
 */
function buyWithShippingRequested() { // eslint-disable-line no-unused-vars
  paymentRequestWithOptions({requestShipping: true, requestPayerName: false,
      requestPayerEmail: false, requestPayerPhone: false});
}

/**
 * Launches the PaymentRequest UI with 'basic-card' and 'bobpay' as payment
 * methods and payer's contact details requested.
 */
function buyWithContactRequested() { // eslint-disable-line no-unused-vars
  paymentRequestWithOptions({requestPayerName: true, requestPayerEmail: true,
      requestPayerPhone: true});
}

/**
 * Launches the PaymentRequest UI with 'basic-card' and 'bobpay' as payment
 * methods and both shipping address and payer's contact details requested.
 */
function buyWithShippingAndContactRequested() { // eslint-disable-line no-unused-vars, max-len
  paymentRequestWithOptions({requestShipping: true, requestPayerName: true,
      requestPayerEmail: true, requestPayerPhone: true});
}
