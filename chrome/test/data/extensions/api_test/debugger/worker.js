// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.addEventListener('connect', function(e) {
  var port = e.ports[0];
  port.start();
  port.postMessage({});
});