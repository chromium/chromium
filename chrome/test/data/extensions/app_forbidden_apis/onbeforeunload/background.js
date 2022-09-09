// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

var didRun = sessionStorage.didRun;

function beforeUnload() {
  chrome.test.fail();
}

try {
  delete window.onbeforeunload;
  window.onbeforeunload = beforeUnload;
} catch (e) {}

try {
  window.addEventListener('beforeunload', beforeUnload);
} catch (e) {}

try {
  var beforeUnloadTricky = {
    toString: function() {
      beforeUnloadTricky.toString = function() { return 'beforeunload'; };
      return 'something not beforeunload';
    }
  };
  window.addEventListener(beforeUnloadTricky, beforeUnload);
} catch (e) {}

if (!didRun) {
  sessionStorage.didRun = true;
  location.reload();
} else {
  chrome.test.succeed();
}
