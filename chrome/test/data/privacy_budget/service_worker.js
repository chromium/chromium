// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.addEventListener("message", (event) => {
  const workerResult = self.navigator.userAgent;
  event.source.postMessage(workerResult);
});
