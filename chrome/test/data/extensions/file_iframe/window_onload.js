// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.onload = function() {
  var iframe = document.getElementById('file_iframe');
  try {
    var url = iframe.contentWindow.location.href
    if (url === 'about:blank')
      chrome.test.sendMessage('denied');
  } catch (e) {
    var expectedError =
        `Failed to read a named property 'href' from 'Location': Blocked a ` +
        `frame with origin "${window.location.origin}" from ` +
        `accessing a cross-origin frame.`;
    if (e.message === expectedError)
      chrome.test.sendMessage('allowed');
  }
};
