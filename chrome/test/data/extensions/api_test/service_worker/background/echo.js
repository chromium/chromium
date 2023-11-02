// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

self.onmessage = function(event) {
  // Expect the event to have a message, and a port to echo the message on.
  event.data.port.postMessage(event.data.message);
};
