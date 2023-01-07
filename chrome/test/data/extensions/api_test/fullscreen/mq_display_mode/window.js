// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
function checkWindowRestored() {
  var standalone = matchMedia( '(display-mode: standalone)' );
  chrome.test.assertTrue(standalone.matches,
    "Display mode of the restored window is 'standalone'");

  chrome.test.succeed();
}

function checkWindowFullscreened() {
  var fullscreen = matchMedia( '(display-mode: fullscreen)' );
  chrome.test.assertTrue(fullscreen.matches,
    "Display mode of the fullscreened window is 'fullscreen'");

  window.onresize = checkWindowRestored;
  chrome.app.window.current().restore();
}

window.onload = function() {
  function checkDisplayModeMediaFeature() {
    var standalone = matchMedia( '(display-mode: standalone)' );
    chrome.test.assertTrue(standalone.matches,
                           "Initially display mode is 'standalone'");
    window.onresize = checkWindowFullscreened;
    chrome.app.window.current().fullscreen();
  };
  chrome.test.runTests([checkDisplayModeMediaFeature]);
}
