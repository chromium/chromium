// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These API calls should pass since geolocation is enabled for this
// extension.
chrome.test.runTests([
  function geolocation_getCurrentPosition() {
    navigator.geolocation.getCurrentPosition(chrome.test.succeed,
                                             chrome.test.fail);
  },
  function geolocation_watchPosition() {
    navigator.geolocation.watchPosition(chrome.test.succeed,
                                        chrome.test.fail);
  }
]);
