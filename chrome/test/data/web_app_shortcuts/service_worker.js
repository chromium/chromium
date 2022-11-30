// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.addEventListener('fetch', e => {
  e.respondWith((async () => {
    try {
      return await fetch(e.request);
    } catch (error) {
      return new Response('Hello offline page');
    }
  })());
});
