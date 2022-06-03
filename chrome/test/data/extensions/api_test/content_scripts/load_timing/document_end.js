// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

if (!window.documentStartRan) {
  console.error('document_end script triggered before document_start!');
  chrome.test.sendMessage('document-end-failure');
} else {
  window.documentEndRan = true;
  chrome.test.sendMessage('document-end-success');
}
