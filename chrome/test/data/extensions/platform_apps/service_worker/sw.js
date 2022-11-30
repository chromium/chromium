// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.onmessage = function(e) {
  if (e.data && e.data.request === 'ping') {
    e.ports[0].postMessage({response: 'pong'});
  }
};
