// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.addEventListener('unload', function() {
  chrome.runtime.sendMessage('Unloading');
});

chrome.test.sendMessage('listening');
