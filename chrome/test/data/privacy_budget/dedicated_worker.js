// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onmessage = (e) => {
  const workerResult = self.navigator.userAgent;
  postMessage(workerResult);
  // Close the worker to flush out the metrics.
  self.close();
}
