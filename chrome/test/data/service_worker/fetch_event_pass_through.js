// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var saw_requests = [];

self.addEventListener('fetch', event => {
  saw_requests.push(event.request.url);
});

self.addEventListener('message', event => {
  event.source.postMessage(saw_requests);
});
