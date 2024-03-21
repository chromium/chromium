// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

if (typeof hasRunContentScriptAtDocumentStart == 'undefined') {
  chrome.runtime.sendMessage('document_start script has not run!');
} else if (window.parent !== window) {
  // Assume iframe
  chrome.runtime.sendMessage('jsresult/' + document.body.textContent.trim());
}
