// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let importScriptsGreeting;
let message = [];

self.onconnect = async function(e) {
  const port = e.ports[0];
  port.start();
  message.push('CONNECTED');

  // The import scripts writes to |importScriptsGreeting|.
  importScripts('shared_worker_import.js');
  message.push(importScriptsGreeting);

  const resp = await fetch(new URL('data_for_fetch', self.location));
  const text = await resp.text();
  message.push(text.trim());

  port.postMessage(message);
};
