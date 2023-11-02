// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

if (!window.documentStartRan || !window.documentEndRan) {
  console.error('document_idle script triggered out of order!');
  chrome.test.sendMessage('document-idle-failure');
} else {
  chrome.test.sendMessage('document-idle-success');
}
