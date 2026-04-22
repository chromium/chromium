// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const SECRET = 'main_window_secret';

onmessage = function(event) {
  const sandboxedWindow = event.source;
  // They can't read our SECRET.
  chrome.test.assertEq(undefined, event.data);

  // And we can't read theirs.
  sandboxedWindowSecret = undefined;
  try {
    sandboxedWindowSecret = sandboxedWindow.SECRET;
  } catch (e) {
  }
  chrome.test.assertEq(undefined, sandboxedWindowSecret);

  chrome.test.succeed();
};

onload = function() {
  chrome.test.runTests([
    function sandboxedWindow() {
      const w = window.open('sandboxed.html');
    },

    function sandboxedFrame() {
      const iframe = document.createElement('iframe');
      iframe.src = 'sandboxed.html';
      document.body.appendChild(iframe);
    },
  ]);
};
