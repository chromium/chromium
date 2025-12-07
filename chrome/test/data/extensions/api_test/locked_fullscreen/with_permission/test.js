// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

openLockedFullscreenWindow = async function() {
  const window = await chrome.windows.create(
      {url: 'about:blank', state: 'locked-fullscreen'});
  chrome.test.assertEq('locked-fullscreen', window.state);
  chrome.test.succeed();
};

openLockedFullscreenWindowWithIncorrectUrlCount = async function() {
  const incorrectUrlCountErrorMessage =
      'When creating a new window in locked fullscreen mode, exactly one URL ' +
      'should be supplied.';

  // Verify error when no URLs are specified.
  await chrome.test.assertPromiseRejects(
      chrome.windows.create({url: [], state: 'locked-fullscreen'}),
      `Error: ${incorrectUrlCountErrorMessage}`);

  // Also verify error when more than one URL is specified.
  await chrome.test.assertPromiseRejects(
      chrome.windows.create({
        url: ['about:blank', 'chrome://version'],
        state: 'locked-fullscreen'
      }),
      `Error: ${incorrectUrlCountErrorMessage}`);
  chrome.test.succeed();
};

updateWindowToLockedFullscreen = async function() {
  const window = await chrome.windows.getCurrent();
  const updatedWindow =
      await chrome.windows.update(window.id, {state: 'locked-fullscreen'});
  chrome.test.assertEq('locked-fullscreen', updatedWindow.state);
  chrome.test.succeed();
};

updateIncompatibleWindowToLockedFullscreen = async function() {
  const window = await chrome.windows.getCurrent();
  const updatedWindow =
      await chrome.windows.update(window.id, {state: 'locked-fullscreen'});
  chrome.test.assertNe('locked-fullscreen', updatedWindow.state);
  chrome.test.succeed();
};

removeLockedFullscreenFromWindow = async function() {
  const window = await chrome.windows.getCurrent();
  chrome.test.assertEq('locked-fullscreen', window.state);

  const updatedWindow =
      await chrome.windows.update(window.id, {state: 'fullscreen'});
  chrome.test.assertEq('fullscreen', updatedWindow.state);
  chrome.test.succeed();
};

removeLockedFullscreenFromIncompatibleWindow = async function() {
  const window = await chrome.windows.getCurrent();
  chrome.test.assertEq('locked-fullscreen', window.state);

  const updatedWindow =
      await chrome.windows.update(window.id, {state: 'fullscreen'});
  chrome.test.assertNe('fullscreen', updatedWindow.state);
  chrome.test.succeed();
};

const tests = {
  openLockedFullscreenWindow,
  openLockedFullscreenWindowWithIncorrectUrlCount,
  updateWindowToLockedFullscreen,
  updateIncompatibleWindowToLockedFullscreen,
  removeLockedFullscreenFromWindow,
  removeLockedFullscreenFromIncompatibleWindow,
};

chrome.test.getConfig(function(config) {
  if (config.customArg in tests)
    chrome.test.runTests([tests[config.customArg]]);
  else
    chrome.test.fail('Test "' + config.customArg + '"" doesnt exist!');
});
