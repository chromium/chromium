// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

self.onmessage = function(e) {
  e.data.port.postMessage(e.data.message);
};
