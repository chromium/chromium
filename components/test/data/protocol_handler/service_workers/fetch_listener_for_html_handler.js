// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.addEventListener('fetch', function(event) {
  if (!event.request.url.includes('handler.html')) {
    return;
  }

  event.respondWith(new Response(
      `<script>
window.opener.postMessage({handled_by_service_worker: true}, '*');</script>`,
      {headers: {'Content-Type': 'text/html'}}));
});
