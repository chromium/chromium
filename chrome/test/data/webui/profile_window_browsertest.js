// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testNoPodFocused() {
  expectEquals(
      null, document.querySelector('.pod.focused'),
      'No pod should be focused.');
}

function testPodFocused(profilePath) {
  var pods = document.querySelectorAll('.pod.focused');
  assertEquals(1, pods.length, 'Exactly one pod should be focused.');
  expectEquals(
      profilePath, pods[0].user.profilePath, 'A wrong pod is focused.');
}
