// Copyright 2009 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

if (window == top) {
  chrome.runtime.connect().postMessage(true);
}
