// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

(async () => {
  let foundClients =
      await clients.matchAll({includeUncontrolled: true, type: 'window'});
  let background =
      foundClients.find((client) => {
        return new URL(client.url).pathname == '/background.html';
      });
  background.postMessage('success');
})();
