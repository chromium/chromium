// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Updates the price based on the selected shipping option.
 * @param {object} details - The current details to update.
 * @param {string} shippingOption - The shipping option selected by user.
 * @return {object} The updated details.
 */
function updateDetails(details, shippingOption) {
  let selectedShippingOption;
  let otherShippingOption;
  if (shippingOption === 'standard') {
    selectedShippingOption = details.shippingOptions[0];
    otherShippingOption = details.shippingOptions[1];
    details.total.amount.value = '55.00';
  } else {
    selectedShippingOption = details.shippingOptions[1];
    otherShippingOption = details.shippingOptions[0];
    details.total.amount.value = '67.00';
  }
  if (details.displayItems.length === 2) {
    details.displayItems.splice(1, 0, selectedShippingOption);
  } else {
    details.displayItems.splice(1, 1, selectedShippingOption);
  }
  selectedShippingOption.selected = true;
  otherShippingOption.selected = false;
  return details;
}

/**
 * Launches payment request that provides multiple shipping options worldwide,
 * regardless of the shipping address.
 */
function onBuyClicked() {
  const supportedInstruments = [
    {
      supportedMethods: 'https://android.com/pay',
      data: {
        merchantName: 'Rouslan Solomakhin',
        merchantId: '00184145120947117657',
        allowedCardNetworks: ['AMEX', 'MASTERCARD', 'VISA', 'DISCOVER'],
        paymentMethodTokenizationParameters: {
          tokenizationType: 'GATEWAY_TOKEN',
          parameters: {
            'gateway': 'stripe',
            'stripe:publishableKey': 'pk_live_lNk21zqKM2BENZENh3rzCUgo',
            'stripe:version': '2016-07-06',
          },
        },
      },
    },
    {
      supportedMethods: 'basic-card',
    },
  ];

  const details = {
    total: {
      label: 'Donation',
      amount: {
        currency: 'USD',
        value: '55.00',
      },
    },
    displayItems: [
      {
        label: 'Original donation amount',
        amount: {
          currency: 'USD',
          value: '65.00',
        },
      },
      {
        label: 'Friends and family discount',
        amount: {
          currency: 'USD',
          value: '-10.00',
        },
      },
    ],
    shippingOptions: [
      {
        id: 'standard',
        label: 'Standard shipping',
        amount: {
          currency: 'USD',
          value: '0.00',
        },
        selected: true,
      },
      {
        id: 'express',
        label: 'Express shipping',
        amount: {
          currency: 'USD',
          value: '12.00',
        },
      },
    ],
  };

  const options = {
    requestShipping: true,
    requestPayerName: true,
    requestPayerPhone: true,
    requestPayerEmail: true,
  };

  if (!window.PaymentRequest) {
    error('PaymentRequest API is not supported.');
    return;
  }

  try {
    const request = new PaymentRequest(supportedInstruments, details, options);

    request.addEventListener('shippingaddresschange', function(e) {
      e.updateWith(new Promise(function(resolve) {
        window.setTimeout(function() {
          // No changes in price based on shipping address change.
          resolve(details);
        }, 2000);
      }));
    });

    request.addEventListener('shippingoptionchange', function(e) {
      e.updateWith(new Promise(function(resolve) {
        resolve(updateDetails(details, request.shippingOption));
      }));
    });

    request.show()
        .then(function(instrumentResponse) {
          window.setTimeout(function() {
            instrumentResponse.complete('success')
                .then(function() {
                  done(
                      'This is a demo website. No payment will be processed.',
                      instrumentResponse);
                })
                .catch(function(err) {
                  error(err);
                });
          }, 2000);
        })
        .catch(function(err) {
          error(err);
        });
  } catch (e) {
    error('Developer mistake: \'' + e.message + '\'');
  }
}
