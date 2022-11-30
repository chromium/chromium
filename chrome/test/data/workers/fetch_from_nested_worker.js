// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const worker = new Worker('fetch_from_worker.js');
worker.onmessage = e => self.postMessage(e.data);
self.onmessage = e => worker.postMessage(e.data);
