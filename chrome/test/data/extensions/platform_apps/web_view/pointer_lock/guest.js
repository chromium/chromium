// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See chrome/browser/extensions/web_view_interactive_browsertest.cc
// (WebViewInteractiveTest, PointerLock) for documentation on this test.

function LockMouse(element) {
  element.requestPointerLock = element.requestPointerLock;
  element.requestPointerLock();
}
var first_lock = true;
document.onpointerlockchange = function() {
  if (document.pointerLockElement) {
    if (first_lock) {
      console.log('locked');
      setTimeout(function() { embedder.postMessage('locked', '*'); }, 500);
    } else {
      console.log('deleting...');
      setTimeout(function() { embedder.postMessage('delete me', '*'); }, 500);
    }
    first_lock = false;
  } else {
    console.log('unlocked');
    embedder.postMessage('unlocked', '*');
  }
}

document.onpointerlockerror = function() {
  console.log('lock error', '*');
  setTimeout(function() {  embedder.postMessage('lock error', '*'); }, 1000);
}

var embedder = null;
window.addEventListener('message', function(e) {
  embedder = e.source;
  embedder.postMessage('connected', '*');
});

document.getElementById('locktarget1').addEventListener('mousemove',
    function (e) {
  setTimeout(function() { embedder.postMessage('mouse-move', '*'); }, 500);
  if (info.innerHTML != 'fail') {
    info.innerHTML = 'Info: movementX: '+ e.movementX +
        ', movementY: ' + e.movementY;
    }
});

document.getElementById('locktarget2')
    .addEventListener('mousemove', function(e) {
      // The mouse move event can be reached eariler than the pointer unlock
      // event, but `pointerLockElement` would be null if the pointer is
      // unlocked. So, we should ignore the mouse move event.
      if (document.pointerLockElement == null && !first_lock)
        return;
      info.innerHTML = 'fail';
      embedder.postMessage('Pointer was not locked to locktarget1.', '*');
    });

document.getElementById('button1').addEventListener('click', function (e) {
  console.log('click captured, locking mouse');
  LockMouse(locktarget1);
}, false);

document.getElementById('button2').addEventListener('click', function (e) {
  console.log('clicked button 2');
  embedder.postMessage('clicked', '*');
}, false);
