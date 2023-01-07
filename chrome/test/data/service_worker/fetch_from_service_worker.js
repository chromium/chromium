// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

async function handleMessage(e) {
  try {
    const response = await fetch(e.data.url);
    if (!response.ok) {
      e.ports[0].postMessage('bad response');
      return;
    }
    const text = await response.text();
    e.ports[0].postMessage(text);
  } catch (error) {
    e.ports[0].postMessage(`${error}`);
  }
}

self.addEventListener('message', e => {
  e.waitUntil(handleMessage(e));
});
