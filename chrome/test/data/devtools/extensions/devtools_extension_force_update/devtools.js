// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.devtools.panels.create('TestPanel', 'icon.png', 'panel.html', () => {
  // Indicates that panel has loaded to C++ test.
  chrome.devtools.inspectedWindow.eval('console.log("PASS")');
});
