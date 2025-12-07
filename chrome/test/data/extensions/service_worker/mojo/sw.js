// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function createMojoMessagePipe() {
    let {handle0, handle1} = Mojo.createMessagePipe();
    handle0.close();
    handle1.close();
    chrome.test.succeed();
  }
]);
