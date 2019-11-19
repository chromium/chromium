// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

if (typeof hasRunContentScriptAtDocumentStart == 'undefined') {
  chrome.extension.sendRequest('document_start script has not run!');
} else if (window.parent !== window) {
  // Assume iframe
  chrome.extension.sendRequest('jsresult/' + document.body.textContent.trim());
}
