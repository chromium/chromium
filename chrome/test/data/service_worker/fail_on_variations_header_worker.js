// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.addEventListener('fetch', event => {
  const req = event.request;
  if (req.headers.has('X-Client-Data'))
    event.respondWith(Response.error());
});
