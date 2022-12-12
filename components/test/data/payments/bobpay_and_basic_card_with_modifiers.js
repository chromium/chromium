/*
 * Copyright 2017 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Launches the PaymentRequest UI with Bob Pay and basic-card as payment
 * methods and a modifier for basic-card.
 */
function buy() {
  try {
    new PaymentRequest(
        [
          {supportedMethods: 'https://bobpay.test'},
          {supportedMethods: 'basic-card'},
        ],
        {
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
                print(error.message);
              });
        })
        .catch(function(error) {
          print(error.message);
        });
  } catch (error) {
    print(error.message);
  }
}

/**
 * Launches the PaymentRequest UI with Bob Pay and basic-card as payment
 * methods and a modifier for Bob Pay.
 */
function buyWithBobPayDiscount() {
  try {
    new PaymentRequest(
        [
          {supportedMethods: 'https://bobpay.test'},
          {supportedMethods: 'basic-card'},
        ],
        {
          total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}},
          modifiers: [{
            supportedMethods: 'https://bobpay.test',
            total: {
              label: 'Total',
              amount: {currency: 'USD', value: '4.00'},
            },
            additionalDisplayItems: [{
              label: 'BobPay discount',
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
                print(error.message);
              });
        })
        .catch(function(error) {
          print(error.message);
        });
  } catch (error) {
    print(error.message);
  }
}

/**
 * Launches the PaymentRequest UI with Bob Pay and basic-card as payment
 * methods and a modifier for basic-card with "visa" network
 */
function visaSupportedNetwork() {
  try {
    new PaymentRequest(
        [
          {supportedMethods: 'https://bobpay.test'},
          {supportedMethods: 'basic-card'},
        ],
        {
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
                print(error.message);
              });
        })
        .catch(function(error) {
          print(error.message);
        });
  } catch (error) {
    print(error.message);
  }
}

/**
 * Launches the PaymentRequest UI with Bob Pay and basic-card as payment
 * methods and a modifier for basic-card with "mastercard" network
 */
function mastercardSupportedNetwork() {
  try {
    new PaymentRequest(
        [
          {supportedMethods: 'https://bobpay.test'},
          {supportedMethods: 'basic-card'},
        ],
        {
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
            data: {
              discountProgramParticipantId: '86328764873265',
              supportedNetworks: ['mastercard'],
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
                print(error.message);
              });
        })
        .catch(function(error) {
          print(error.message);
        });
  } catch (error) {
    print(error.message);
  }
}

/**
 * Launches the PaymentRequest UI with Bob Pay and basic-card as payment
 * methods and a modifier for basic-card with "mastercard" network.
 */
function mastercardAnySupportedType() {
  try {
    new PaymentRequest(
        [
          {supportedMethods: 'https://bobpay.test'},
          {supportedMethods: 'basic-card'},
        ],
        {
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
            data: {
              discountProgramParticipantId: '86328764873265',
              supportedNetworks: ['mastercard'],
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
                print(error.message);
              });
        })
        .catch(function(error) {
          print(error.message);
        });
  } catch (error) {
    print(error.message);
  }
}

/**
 * Launches the PaymentRequest UI with basic-card as payment method and a
 * modifier for basic-card with "mastercard" network, but the modifier does not
 * have a total specified.
 */
function noTotal() {
  try {
    new PaymentRequest([{supportedMethods: 'basic-card'}], {
      total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}},
      modifiers: [
        {
          supportedMethods: 'basic-card',
          data: {
            mastercardProgramParticipantId: '86328764873265',
            supportedNetworks: ['mastercard'],
          },
        },
      ],
    })
        .show()
        .then(function(resp) {
          resp.complete('success')
              .then(function() {
                print(JSON.stringify(resp, undefined, 2));
              })
              .catch(function(error) {
                print(error.message);
              });
        })
        .catch(function(error) {
          print(error.message);
        });
  } catch (error) {
    print(error.message);
  }
}
