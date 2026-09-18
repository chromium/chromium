// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

importScripts('guest_sw_fetch_worker.js');

self.addEventListener('install', e => {
  self.skipWaiting();
});

self.addEventListener('activate', e => {
  e.waitUntil(clients.claim());
});

self.addEventListener('message', e => {
  if (e.data === 'FETCH') {
    fetch('guest_sw_fetch_from_worker.txt').then(() => {
      e.source.postMessage('FETCH_DONE');
    });
  }
});
