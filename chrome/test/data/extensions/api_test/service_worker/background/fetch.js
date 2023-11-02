// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

self.onfetch = function(event) {
  let url = new URL(event.request.url);
  event.respondWith(new Response('Caught a fetch for ' + url.pathname));
};
