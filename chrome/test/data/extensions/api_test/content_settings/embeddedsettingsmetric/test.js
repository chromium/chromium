// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var cs = chrome.contentSettings;

chrome.test.runTests([
  function setEmbeddedSettings() {
    let callsCompleted = 0;
    function setComplete() {
      ++callsCompleted;
      if (callsCompleted == 4) {
        chrome.test.succeed()
      }
    }

    // Embedded patterns.
    cs['images'].set({
      'primaryPattern': 'http://google.com/*',
      'secondaryPattern': 'http://example.com/*',
      'setting': 'allow'
    }, setComplete);
    cs['location'].set({
      'primaryPattern': 'http://google.com/*',
      'secondaryPattern': 'http://example.com/*',
      'setting': 'allow'
    }, setComplete);

    // Top level patterns.
    cs['images'].set({
      'primaryPattern': 'http://google.com/*',
      'secondaryPattern': 'http://google.com/*',
      'setting': 'allow'
    }, setComplete);
    cs['cookies'].set({
      'primaryPattern': 'http://google.com/*',
      'secondaryPattern': '<all_urls>',
      'setting': 'allow'
    }, setComplete);
  },
]);
