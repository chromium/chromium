// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const error_msg =
    'Cannot lock window to fullscreen or close a locked fullscreen ' +
    'window without lockWindowFullscreenPrivate manifest permission';

openLockedFullscreenWindow = function() {
  chrome.windows.create({url: 'about:blank', state: 'locked-fullscreen'},
    chrome.test.callbackFail(error_msg));
};

updateWindowToLockedFullscreen = function() {
  chrome.windows.getCurrent(null, function(window) {
    chrome.windows.update(window.id, {state: 'locked-fullscreen'},
      chrome.test.callbackFail(error_msg))});
};

removeLockedFullscreenFromWindow = function() {
  chrome.windows.getCurrent(null, function(window) {
    chrome.windows.update(window.id, {state: 'fullscreen'},
      chrome.test.callbackFail(error_msg));
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
