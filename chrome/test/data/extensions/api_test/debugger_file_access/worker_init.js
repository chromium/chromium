// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async () => {
  const blob = new Blob(
      ['console.log("Worker running"); setInterval(() => {}, 1000);'],
      {type: 'application/javascript'});
  const url = URL.createObjectURL(blob);
  new Worker(url, {name: 'MyBlobWorker'});
  document.title = 'worker-created';
})();
