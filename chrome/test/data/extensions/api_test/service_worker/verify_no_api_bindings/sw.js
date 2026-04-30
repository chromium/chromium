// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const expectedKeys = ['csi', 'loadTimes', 'runtime'];

self.onmessage = function(e) {
  let message = 'SUCCESS';
  if (e.data !== 'checkBindingsTest') {
    message = 'FAILURE';
  } else {
    for (const key in chrome) {
      if (!expectedKeys.includes(key)) {
        console.info(`Unexpected key: ${key}`);
        message = 'FAILURE';
        break;
      }
    }
  }
  e.ports[0].postMessage(message);
};
