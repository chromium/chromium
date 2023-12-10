// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.storage.onChanged.addListener(function(changes, namespace) {
  if (namespace === 'session' && changes.notify?.newValue === 'yes') {
    chrome.runtime.sendMessage('storage.session.onChanged received');
  } else if (namespace === 'local' && changes.notify?.newValue === 'yes') {
    chrome.runtime.sendMessage('storage.local.onChanged received');
  } else {
    chrome.runtime.sendMessage('script received an unexpected message');
  }
})
