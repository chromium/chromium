// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async () => {
  const blob = new Blob([], {type: 'application/javascript'});
  const url = URL.createObjectURL(blob);
  new Worker(url);
  document.title = 'worker-created';
})();
