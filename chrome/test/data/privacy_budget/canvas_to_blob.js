// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Gets an image blob from a HTMLCanvasElement and sends it back to the test
// via `sendValueToTest`.
window.addEventListener('load', () => {
  document.createElement('canvas').toBlob(function(blob) {
    sendValueToTest(JSON.stringify({
      'type': blob.type,
    }));
  });
});
