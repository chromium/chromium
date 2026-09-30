// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.addEventListener('fetch', event => {
  const headers = new Headers(event.request.headers);
  headers.set('X-Fallback-Service-Worker', '1');
  event.respondWith(
      fetch(new Request(event.request, {headers, cache: 'no-store'})));
});
