/*
 * Copyright 2022 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/** {object} - The "canmakepayment" event field check results. */
let details = {};

self.addEventListener('canmakepayment', (event) => {
  // "if" condition checks.
  details.ifTopOrigin = (!!event.topOrigin);
  details.ifPaymentRequestOrigin = (!!event.paymentRequestOrigin);
  details.ifMethodData = (!!event.methodData);
  details.ifModifiers = (!!event.modifiers);

  // Comparisons to empty values.
  details.emptyTopOrigin = (event.topOrigin === '');
  details.emptyPaymentRequestOrigin = (event.paymentRequestOrigin === '');
  details.emptyMethodData = (!event.methodData || event.methodData.length == 0);
  details.emptyModifiers = (!event.modifiers || event.modifiers.length == 0);

  // Comparison to "undefined".
  details.definedTopOrigin = (event.topOrigin !== undefined);
  details.definedPaymentRequestOrigin =
      (event.paymentRequestOrigin !== undefined);
  details.definedMethodData = (event.methodData !== undefined);
  details.definedModifiers = (event.modifiers !== undefined);

  event.respondWith(true);
});

self.addEventListener('paymentrequest', (event) => {
  event.respondWith({
    methodName: event.methodData[0].supportedMethods,
    details,
  });
});
