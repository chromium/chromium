// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let count = 0;

function markPlaying() {
  // Signal "Ready" (the C++ TitleWatcher handshake) when all elements play.
  if (--count == 0)
    window.document.title = "Ready";
}

function register(...tagNames) {
  const elements =
      tagNames.flatMap(t => [...document.getElementsByTagName(t)]);

  // Count all elements up front to avoid signalling "Ready" too early.
  count = elements.length;
  for (const el of elements) {
    // Already playing: the late script missed 'play', so count it now.
    if (!el.paused)
      markPlaying();
    else
      el.addEventListener('play', markPlaying, { once: true });
  }
}

register('video', 'audio');
