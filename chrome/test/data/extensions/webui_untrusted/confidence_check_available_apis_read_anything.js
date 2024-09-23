// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// out/Debug/browser_tests --gtest_filter=ExtensionUntrustedWebUITest.\
//     ConfidenceCheckAvailableAPIsReadAnything


// There should be a limited number of chrome.* APIs available to the
// ReadAnything untrusted WebUI. Confidence check them here.
//
// NOTE: Of course, update this list if/when more APIs are made available.

var expected = [
  // Deprecated proprietary Chrome APIs unrelated to Extensions.
  'csi',
  'loadTimes',
  // For logging various activity in
  // chrome-untrusted://read-anything-side-panel.top-chrome.
  'metricsPrivate',
  // chrome.readingMode is available in
  // chrome-untrusted://read-anything-side-panel.top-chrome.
  'readingMode',
  // chrome.runtime is always available for chrome-untrusted://.
  'runtime',
];

var actual = Object.keys(chrome).sort();

var isEqual = expected.length == actual.length;
for (var i = 0; i < expected.length && isEqual; i++) {
  if (expected[i] != actual[i])
    isEqual = false;
}

if (!isEqual) {
  console.error(window.location.href + ': ' +
                'Expected: ' + JSON.stringify(expected) + ', ' +
                'Actual: ' + JSON.stringify(actual));
}
return isEqual;
