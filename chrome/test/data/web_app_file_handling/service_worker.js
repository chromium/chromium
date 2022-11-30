// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.addEventListener('install', function() {
  // Activate worker immediately.
  return self.skipWaiting();
});

self.addEventListener('fetch', function(event) {
  event.respondWith(fetch(event.request));
});
