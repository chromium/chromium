// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

console.log('ServiceWorker executed');

self.addEventListener('install', function (event) {
  console.log('ServiceWorker install');
});

self.addEventListener('fetch', function (event) {
  // Replace main page with one that includes an image tag that can be
  // preload scanned and prefetched.
  console.log('Saw request ' + event.request.url);
  if (event.request.url.endsWith('prerender/prefetch_page.html')) {
    console.log('Intercepting ' + event.request.url);
    var headers = new Headers;
    headers.set('Content-Type', 'text/html; charset=UTF-8');
    var content = '<html><body><img src="/prerender/image.png"/></body></html>';
    var response = new Response(content, {
      status: 200,
      headers: headers });
    event.respondWith(response);
  }
});
