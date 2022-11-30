// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.addEventListener('fetch', e => {
  // Avoid to handle a request if the url contains an ignore parameter.
  if (e.request.url.indexOf('?ignore') == -1) {
    e.respondWith((async () => {
      try {
        return await fetch(e.request);
      } catch (error) {
        return new Response('Hello offline page');
      }
    })());
  }
});
