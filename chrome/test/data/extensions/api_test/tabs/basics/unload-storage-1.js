// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.addEventListener('unload', (e) => {
  chrome.storage.local.set({'did_run_unload_1': 'yes'});
});
