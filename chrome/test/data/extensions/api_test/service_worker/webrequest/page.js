// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function register() {
  navigator.serviceWorker.register('sw.js')
    .then(() => navigator.serviceWorker.ready)
    .then(() => window.domAutomationController.send('ready'));
}

async function doFetch(url) {
  const response = await fetch(url);
  const text = await response.text();
  window.domAutomationController.send(text.trim());
}
