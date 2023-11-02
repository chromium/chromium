// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var tests = [
  function isNotInIncognitoMode() {
    chrome.webstorePrivate.isInIncognitoMode(callbackPass(function(incognito) {
      assertFalse(incognito);
    }));
  },
];

runTests(tests);
