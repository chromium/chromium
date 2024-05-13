// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// out/Debug/browser_tests \
//    --gtest_filter=ExtensionWebUITest.ConfidenceCheckAvailableAPIs

// There should be a limited number of chrome.* APIs available to webui.
// Confidence check them here.
//
// NOTE:  Of course, update this list if/when more APIs are made available.
// NOTE2: Apologies to other people putting things on chrome which aren't
//        related and whose tests fail.

var expected = [
  'activityLogPrivate',
  'csi',
  'developerPrivate',
  'getVariableValue',
  'loadTimes',
  'management',
  'metricsPrivate',
  'runtime',
  'send',
  'test',
  'timeTicks'
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
