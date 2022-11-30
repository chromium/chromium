// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test that both static <iframe> tags and dynamically generated ones can be
// loaded by their parent extension (iframe-contents.html is not listed in
// web_accessible_resources).
var staticIframeLoaded = false;
function iframeLoaded() {
  if (staticIframeLoaded) {
    chrome.test.notifyPass();
    return;
  }

  staticIframeLoaded = true;

  var iframe = document.createElement('iframe');
  document.body.appendChild(iframe);
  iframe.src = 'iframe-contents.html';
}
