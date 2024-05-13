// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// out/Debug/browser_tests \
//    --gtest_filter=ExtensionUntrustedWebUITest.ConfidenceCheckAvailableAPIs

// There should be a limited number of chrome.* APIs available to untrusted
// WebUIs. Confidence check them here.
//
// NOTE: Of course, update this list if/when more APIs are made available.

var expected = [
  // Deprecated proprietary Chrome APIs unrelated to Extensions.
  'csi',
  'loadTimes',
  // chrome.runtime is always available for chrome-untrusted://.
  'runtime',
  // chrome.test is granted for testing.
  'test',
];

var actual = Object.keys(chrome).sort();
chrome.test.assertEq(expected, actual);
chrome.test.notifyPass();
