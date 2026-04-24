// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.runtime.sendMessage({
  source: location.hostname,
  modified: window.title == 'Hello',
});
