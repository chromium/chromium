// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The helper function to select the element specified by id.
function selectText(id) {
  const input = document.getElementById(id);
  var range = document.createRange();
  range.selectNodeContents(input);
  var selection = window.getSelection();
  selection.removeAllRanges();
  selection.addRange(range);
}

function copyToClipboard() {
  document.execCommand('copy');
}
