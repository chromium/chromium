// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var parentWindow = null;

window.onload = () => {
  document.body.addEventListener('wheel', (e) => {
    parentWindow.postMessage('Seen wheel event', '*');
  });
};

window.addEventListener('message', (e) => {
  parentWindow = e.source;

  // We need to wait for the compositor thread to be made aware of the wheel
  // listener before sending the pinch event sequence.
  window.requestAnimationFrame(() => {
    window.requestAnimationFrame(() => {
      parentWindow.postMessage('WebViewTest.LAUNCHED', '*');
    });
  });
});
