// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is intended to be served at the root of a server to allow
// isolation. Thus the scope of the serviceworker and resources is at the root,
// and not in the sub directory here

'use strict';

self.addEventListener('fetch', event => {
  event.respondWith(fetch(event.request).catch(_ => {
    return new Response('Offline test.');
  }));
});
