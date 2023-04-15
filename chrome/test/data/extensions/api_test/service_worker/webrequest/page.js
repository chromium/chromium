// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function register() {
  return navigator.serviceWorker.register('sw.js')
    .then(() => navigator.serviceWorker.ready)
    .then(() => 'ready');
}

async function doFetch(url) {
  const response = await fetch(url);
  const text = await response.text();
  return text.trim();
}
