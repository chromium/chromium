// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A worker that imports "empty.js" and also accepts a URL parameter like
// "?import=script.js" and imports "script.js".

const url = new URL(self.location);
const import_url = url.searchParams.get('import');

importScripts('empty.js');
importScripts(import_url);

self.onconnect = e => {
  const port = e.ports[0];
  port.postMessage('DONE');
};
