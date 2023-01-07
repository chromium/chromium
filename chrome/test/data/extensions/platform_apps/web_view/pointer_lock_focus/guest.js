// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See chrome/browser/extensions/web_view_interactive_browsertest.cc
// (WebViewInteractiveTest, PointerLockFocus) for documentation on this test.

function LockMouse(element) {
  element.requestPointerLock = element.requestPointerLock;
  element.requestPointerLock();
}
document.onpointerlockchange = function() {
  if (document.pointerLockElement) {
    console.log('locked');
    setTimeout(function() { embedder.postMessage('locked', '*'); }, 500);
  } else {
    console.log('unlocked');
    setTimeout(function() { embedder.postMessage('unlocked', '*'); }, 500);
  }
}

var embedder = null;
window.addEventListener('message', function(e) {
  embedder = e.source;
  embedder.postMessage('connected', '*');
});

document.getElementById('button1').addEventListener('click', function (e) {
  console.log('click captured, locking mouse');
  LockMouse(locktarget);
}, false);
