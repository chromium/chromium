// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// When the embedder is the primary main frame, the OOPIF template is
// the top frame and there is no host page to coordinate with -- just
// auto-abort. When the embedder is an iframe, defer to the host page
// (it may want to selectively abort one of several handlers).
if (window.parent === window.top) {
  chrome.mimeHandler.abortAndFallbackToNativeHandler();
} else {
  window.addEventListener('message', (event) => {
    if (event.data && event.data.type === 'handler-action' &&
        event.data.action === 'abort') {
      chrome.mimeHandler.abortAndFallbackToNativeHandler();
    }
  });
  window.top.postMessage({type: 'handler-ready'}, '*');
}
