// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var tests = [
  // Tries to enable a disabled extension.
  function enable() {
    chrome.management.getAll(function(items) {
      var disabledItem = getItemNamed(items, 'disabled_extension');
      var expectedError =
          `Extension ${disabledItem.id} cannot be modified by user.`;
      checkItem(disabledItem, 'disabled_extension', false, 'extension');
      chrome.management.setEnabled(disabledItem.id, true, function() {
        assertLastError(expectedError);
        chrome.management.get(disabledItem.id, function(stillDisabledItem) {
          checkItem(
              stillDisabledItem, 'disabled_extension', false, 'extension');
          succeed();
        });
      });
    });
  }
];

chrome.test.runTests(tests);
