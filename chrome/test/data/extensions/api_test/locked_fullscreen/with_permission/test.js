// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const error_msg =
    'Cannot lock window to fullscreen or close a locked fullscreen ' +
    'window without lockWindowFullscreenPrivate manifest permission';

openLockedFullscreenWindow = function() {
  chrome.windows.create({url: 'about:blank', state: 'locked-fullscreen'},
    chrome.test.callbackPass(function(window) {
      // Make sure the new window state is correctly set.
      chrome.test.assertEq('locked-fullscreen', window.state);
  }));
};

updateWindowToLockedFullscreen = function() {
  chrome.windows.getCurrent(null, function(window) {
    chrome.windows.update(window.id, {state: 'locked-fullscreen'},
      chrome.test.callbackPass(function(window) {
        // Make sure the new window state is correctly set.
        chrome.test.assertEq('locked-fullscreen', window.state);
  }))});
};

removeLockedFullscreenFromWindow = function() {
  chrome.windows.getCurrent(null, function(window) {
    chrome.windows.update(window.id, {state: 'fullscreen'},
      chrome.test.callbackPass(function(window) {
        chrome.test.assertEq('fullscreen', window.state);
    }));
  });
};

const tests = {
  openLockedFullscreenWindow: openLockedFullscreenWindow,
  updateWindowToLockedFullscreen: updateWindowToLockedFullscreen,
  removeLockedFullscreenFromWindow: removeLockedFullscreenFromWindow,
};

window.onload = function() {
  chrome.test.getConfig(function(config) {
    if (config.customArg in tests)
      chrome.test.runTests([tests[config.customArg]]);
    else
      chrome.test.fail('Test "' + config.customArg + '"" doesnt exist!');
  });
};
