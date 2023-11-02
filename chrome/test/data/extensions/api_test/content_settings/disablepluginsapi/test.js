// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var cs = chrome.contentSettings;

function setPluginsSetting() {
  cs['plugins'].set(
      {
        'primaryPattern': 'https://www.example.com/*',
        'secondaryPattern': '<all_urls>',
        'setting': 'allow'
      },
      () => {
        chrome.test.assertNoLastError();
        chrome.test.succeed();
      });
}