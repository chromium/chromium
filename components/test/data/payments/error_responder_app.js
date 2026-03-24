/*
 * Copyright 2026 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

self.addEventListener('canmakepayment', (evt) => {
  evt.respondWith(true);
});

self.addEventListener('paymentrequest', (evt) => {
  const errorType = evt.methodData[0].data.errorType;
  if (errorType === 'reject') {
    evt.respondWith(Promise.reject(new Error('Rejected')));
  } else if (errorType === 'operation_error') {
    evt.respondWith(
        Promise.reject(new DOMException('Internal Error', 'OperationError')));
  } else {
    evt.respondWith(Promise.resolve({
      methodName: evt.methodData[0].supportedMethods,
      details: {status: 'success'},
    }));
  }
});
