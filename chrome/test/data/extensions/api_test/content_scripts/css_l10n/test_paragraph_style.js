// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests that the 'p' element has had CSS injected, and that the CSS code
// has had the __MSG_text_color__ message replaced ('text_color' must
// not be present in any CSS code).

var message = 'Test failed to complete';
try {
  var p = document.getElementById('pId');
  var color = getComputedStyle(p).color;
  if (getComputedStyle(p).color == "rgb(255, 0, 0)")
    message = 'passed';
  else
    message = 'Paragraph is not red: ' + color;
} finally {
  chrome.runtime.sendMessage({tag: 'paragraph_style', message: message});
}
