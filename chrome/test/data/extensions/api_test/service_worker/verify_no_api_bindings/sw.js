// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var expectedKeys = ['csi', 'loadTimes', 'runtime'];

self.onmessage = function(e) {
  var message = 'SUCCESS';
  if (e.data != 'checkBindingsTest') {
    message = 'FAILURE';
  } else {
    for (var key in chrome) {
      if (!expectedKeys.includes(key)) {
        console.log('Unexpected key: ' + key);
        message = 'FAILURE';
        break;
      }
    }
  }
  e.ports[0].postMessage(message);
};
