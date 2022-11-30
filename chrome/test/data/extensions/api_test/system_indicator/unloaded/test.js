// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

chrome.test.runTests([function unload_then_click() {
  chrome.systemIndicator.enable();
}]);

chrome.systemIndicator.onClicked.addListener(function() {
    chrome.test.succeed();
});
