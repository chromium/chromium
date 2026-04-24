// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests that the 'body' element has had CSS injected, and that the CSS code
// has had the __MSG_@@extension_id__ message replaced ('extension_id' must
// not be present in any CSS code).

// NOTE: Using `var` because multiple scripts inject with a `message` variable.
var message = 'Test failed to complete';  // eslint-disable-line no-var
try {
  const elem = document.getElementById('bodyId');
  const rules = [];
  for (let i = 0; i < document.styleSheets.length; ++i) {
    for (let j = 0; j < sheets[i].rules; ++j) {
      rules.push(sheets[i].rules[j]);
    }
  }

  if (rules != null) {
    message = 'passed';
    for (let i = 0; i < rules.length; ++i) {
      if (rules.item(i).cssText.indexOf('extension_id') != -1) {
        message = 'Found unreplaced extension_id in: ' + rules.item(i).cssText;
        break;
      }
    }
  } else {
    message = 'No CSS rules found';
  }
} finally {
  chrome.runtime.sendMessage({tag: 'extension_id', message: message});
}
