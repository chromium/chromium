// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onconnect = (e) => {
  const port = e.ports[0];
  port.onmessage = (e) => {
    if (e.data == 'close') {
      // Close the worker to flush out the metrics.
      self.close();
    } else {
      const workerResult = self.navigator.userAgent;
      port.postMessage(workerResult);
    }
  }
}
