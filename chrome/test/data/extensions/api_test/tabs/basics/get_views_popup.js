// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function canGetViewsOfEmptyPopup() {
    testGetNewWindowView({type: 'popup'}, []);
  },

  function canGetViewsOfPopupWithUrl() {
    const urls = ['a.html'];
    testGetNewWindowView({type: 'popup', url: urls}, urls);
  }
]);
