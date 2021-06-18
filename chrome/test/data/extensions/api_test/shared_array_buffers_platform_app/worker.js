// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function verifyData(data) {
  if (data.byteLength != 16)
    return `Improper byteLength: ${data.byteLength}`;

  const bufView = new Uint8Array(data);
  for (let i = 0; i < 16; i++) {
    if (bufView[i] != i % 2) {
      return `Data mismatch at index ${i}: Expected: ${i % 2}, got: ${
          bufView[i]}`;
    }
  }

  return 'PASS';
}

self.addEventListener('message', e => {
  try {
    postMessage(verifyData(e.data));
  } catch (e) {
    postMessage(e.message);
  }
});
