// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const CACHE_NAME = 'request-cache-v1';

self.addEventListener('install', evt => {
  evt.waitUntil(async function() {
    const url = '/test.js'
    const cache = await caches.open(CACHE_NAME);
    const customHeaders = new Headers();
    customHeaders.append('Content-Type', 'text/html');
    const responseBody = 'test successful';
    const response = new Response(responseBody, {
      headers: customHeaders
    });
    await cache.put(url, response);
  }());
});
