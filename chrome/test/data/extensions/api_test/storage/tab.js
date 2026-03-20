// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([function tab() {
  // Check that the localstorage stuff we stored is still there.
  chrome.test.assertTrue(localStorage.foo == 'bar');
}]);
