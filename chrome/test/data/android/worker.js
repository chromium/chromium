// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is accessed via content:// URL and can not be in a sub-directory
// as TestContentProvider does not support it.
onmessage = function(e) {
  postMessage('load');
}
