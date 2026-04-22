// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const tests = [
  // Tries to enable a disabled extension.
  function enable() {
    const onEnabledPromise = new Promise((resolve) => {
      chrome.management.onEnabled.addListener(function(info) {
        assertEq(info.name, 'disabled_extension');
        resolve();
      });
    });
    const setEnabledPromise = new Promise((resolve) => {
      chrome.management.getAll(function(items) {
        const disabledItem = getItemNamed(items, 'disabled_extension');
        checkItem(disabledItem, 'disabled_extension', false, 'extension');
        chrome.management.setEnabled(disabledItem.id, true, function() {
          chrome.management.get(disabledItem.id, function(nowEnabledItem) {
            checkItem(nowEnabledItem, 'disabled_extension', true, 'extension');
            resolve();
          });
        });
      });
    });
    Promise.all([onEnabledPromise, setEnabledPromise]).then(() => {
      succeed();
    });
  },
];

chrome.test.runTests(tests);
