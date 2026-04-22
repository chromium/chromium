// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const errorMsg =
    'Cannot lock window to fullscreen or close a locked fullscreen ' +
    'window without lockWindowFullscreenPrivate manifest permission';

const openLockedFullscreenWindow = async function() {
  await chrome.test.assertPromiseRejects(
      chrome.windows.create({url: 'about:blank', state: 'locked-fullscreen'}),
      `Error: ${errorMsg}`);
  chrome.test.succeed();
};

const updateWindowToLockedFullscreen = async function() {
  const window = await chrome.windows.getCurrent();
  await chrome.test.assertPromiseRejects(
      chrome.windows.update(window.id, {state: 'locked-fullscreen'}),
      `Error: ${errorMsg}`);
  chrome.test.succeed();
};

const removeLockedFullscreenFromWindow = async function() {
  const window = await chrome.windows.getCurrent();
  chrome.test.assertEq('locked-fullscreen', window.state);

  await chrome.test.assertPromiseRejects(
      chrome.windows.update(window.id, {state: 'fullscreen'}),
      `Error: ${errorMsg}`);
  chrome.test.succeed();
};

const tests = {
  openLockedFullscreenWindow,
  updateWindowToLockedFullscreen,
  removeLockedFullscreenFromWindow,
};

chrome.test.getConfig(function(config) {
  if (config.customArg in tests) {
    chrome.test.runTests([tests[config.customArg]]);
  } else {
    chrome.test.fail(`Test "${config.customArg}"" doesnt exist!`);
  }
});
