// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  async function testCreateWithActiveTrue() {
    // TODO(https://crbug.com/511254270): `NavigateParams::tabstrip_add_types`
    // with ADD_ACTIVE isn't fully supported on desktop android yet.
    if ((await chrome.runtime.getPlatformInfo()).os === 'android') {
      chrome.test.succeed('skipped');
      return;
    }
    chrome.test.listenOnce(chrome.tabs.onCreated, function(tab) {
      chrome.test.assertEq(tab.active, true);
    });
    chrome.tabs.create({url: 'chrome://newtab/', active: true});
  },

  function testCreateWithActiveFalse() {
    chrome.test.listenOnce(chrome.tabs.onCreated, function(tab) {
      chrome.test.assertEq(tab.active, false);
    });
    chrome.tabs.create({url: 'chrome://newtab/', active: false});
  },
]);
