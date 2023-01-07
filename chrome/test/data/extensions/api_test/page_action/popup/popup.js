// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Wait to be resized by the browser once before indicating pass.
function onResize() {
  window.removeEventListener("resize", onResize);
  chrome.test.notifyPass();
}

window.addEventListener("resize", onResize);
