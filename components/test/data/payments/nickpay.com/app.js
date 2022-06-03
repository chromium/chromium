/*
 * Copyright 2020 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

self.addEventListener('canmakepayment', (event) => {
  event.respondWith(true);
});

self.addEventListener('paymentrequest', (event) => {
  event.respondWith({
    methodName: event.methodData[0].supportedMethods,
    details: {status: 'success'},
  });
});
