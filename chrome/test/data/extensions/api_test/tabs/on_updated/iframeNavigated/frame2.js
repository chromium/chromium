// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.onload = function() {
  // Navigate after onload so that 'complete' status will fire.
  setTimeout(function() {
    location.href = "iframe3.html";
  }, 0);
}
