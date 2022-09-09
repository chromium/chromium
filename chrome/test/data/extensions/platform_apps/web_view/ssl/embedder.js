// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var webview;

window.createGuest = function() {
  webview = document.createElement('webview');
  webview.src = 'about:blank';
  // Delay the message to `loadstop` so that the test listenser does
  // not pick up the `DidStopLoading` from the initial navigation to
  // `about:blank`.
  webview.addEventListener('loadstop', function() {
    chrome.test.sendMessage('GuestAddedToDom');
  });
  document.body.appendChild(webview);
}

window.loadGuestUrl = function(url) {
  window.console.log('embedder.loadGuest: ' + url);
  webview.setAttribute('src', url);
  webview.style.position = 'fixed';
  webview.style.left = '0px';
  webview.style.top = '0px';
};

window.onload = function() {
  chrome.test.sendMessage('EmbedderLoaded');
};
