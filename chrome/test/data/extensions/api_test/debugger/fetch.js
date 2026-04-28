// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.addEventListener('load', () => {
  fetch('/invalid_char.html').catch(err => {
    console.err(`fetch('/invalid_char.html') failed with error: ${err}`);
  });
});
