// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var worker = new Worker("user_agent.js");
worker.onmessage = function(e) {
  postMessage(e.data);
};
