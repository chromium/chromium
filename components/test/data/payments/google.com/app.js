/*
 * Copyright 2019 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Create a PaymentHandlerResponse that encapsulates an error message. This
 * makes it easier to debug test failures due to errors in JavaScript.
 *
 * @param {string} methodName The method name to use in the created response.
 * @param {string} error The error message to send back.
 * @return {PaymentHandlerResponse} A new PaymentResponse.
 */
function makeErrorResponse(methodName, error) {
  return {
    methodName,
    details: {
      error,
    },
  };
}

/**
 * This payment handler simulates the GPay API.
 */
self.addEventListener('paymentrequest', (evt) => {
  const methodName = evt.methodData[0].supportedMethods;
  const gpayData = evt.methodData[0].data;

  if (methodName != 'https://google.com/pay') {
    const error = `Unexpected payment method. Got: ${methodName}`;
    evt.respondWith(makeErrorResponse(methodName, error));
    return;
  }

  const apiVersion = gpayData['apiVersion'];
  if (apiVersion != 1 && apiVersion != 2) {
    const error = `Unexpected api version. Got: ${apiVersion}`;
    evt.respondWith(makeErrorResponse(methodName, error));
    return;
  }

  const details = {apiVersion};

  if (gpayData['shippingAddressRequired']) {
    details['shippingAddress'] = {
      address1: '123 Main Street',
      address2: 'Unit A',
      address3: '',
      postalCode: '12345',
      companyName: '',
      locality: 'Toronto',
      administrativeArea: 'ON',
      sortingCode: '',
      countryCode: 'CA',
      name: 'Browser Test',
    };
  }

  if (gpayData['emailRequired']) {
    details['email'] = 'paymentrequest@chromium.org';
  }

  if (apiVersion == 1) {
    const cardRequirements = gpayData['cardRequirements'] || {};
    const billingAddressRequired = cardRequirements['billingAddressRequired'];
    if (billingAddressRequired) {
      details['cardInfo'] = {
        billingAddress: {
          countryCode: 'CA',
          postalCode: '12345',
          name: 'Browser Test',
        },
      };
    }
    if (gpayData['phoneNumberRequired'] && billingAddressRequired) {
      details.cardInfo.billingAddress['phoneNumber'] = '+1 234-567-8900';
    }
  } else if (apiVersion == 2) {
    const allowedPaymentMethods = gpayData['allowedPaymentMethods'] || [];
    const cardParameters = allowedPaymentMethods[0]['parameters'] || {};
    const billingAddressRequired = cardParameters['billingAddressRequired'];
    if (billingAddressRequired) {
      details['paymentMethodData'] = {
        type: 'CARD',
        info: {
          billingAddress: {
            countryCode: 'CA',
            postalCode: '12345',
            name: 'BrowserTest',
          },
        },
      };
    }

    const billingAddressParameters =
        cardParameters['billingAddressParameters'] || {};
    if (billingAddressParameters['phoneNumberRequired'] &&
        billingAddressRequired) {
      details.paymentMethodData.info.billingAddress['phoneNumber'] =
          '+1 234-567-8900';
    }
  }

  evt.respondWith({
    methodName,
    details,
  });
});
