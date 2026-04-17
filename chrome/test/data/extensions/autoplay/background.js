// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function autoplay_allowed_without_gesture() {
    const audio = new Audio();
    audio.src = 'test.mp4';
    let allowed = false;
    audio.play()
        .then(
            function() {
              allowed = true;
            },
            function(e) {
              allowed = e.name != 'NotAllowedError';
            })
        .then(function() {
          chrome.test.assertTrue(allowed);
          chrome.test.succeed();
        });
  },
]);
