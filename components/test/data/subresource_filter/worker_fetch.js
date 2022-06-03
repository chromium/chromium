// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

fetch('./worker_fetch_data.txt')
  .then(_ => postMessage('FetchSucceeded'),
        _ => postMessage('FetchFailed'));
