// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// browser_test waits for worker activation to complete before it shuts down
// this worker.  //content API (StopServiceWorkerForScope) used for shutdown
// DCHECKs if the worker hasn't finished installation.
this.addEventListener('activate', function(e) {
  console.log('activate');
  chrome.test.sendMessage('WORKER_ACTIVATED');
});
