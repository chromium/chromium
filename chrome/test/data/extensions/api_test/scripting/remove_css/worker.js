// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

// The original color of the page that we're injecting into.
const ORIGINAL_COLOR = 'rgb(255, 0, 0)';  // red

// The color we inject into the page (first).
const INJECTED_COLOR = 'rgb(0, 128, 0)';  // green

// A secondary color we inject into the page (so that we can differentiate
// between the original and first injected color).
const INJECTED_COLOR2 = 'rgb(255, 255, 0)';  // yellow

// CSS to inject, corresopnding to `INJECTED_COLOR`.
const CSS = '#main { color: green !important; }';
// CSS to inject, corresponding to `INJECTED_COLOR2`.
const CSS2 = CSS.replace('green', 'yellow');
// A file to inject. This also sets the color to `INJECTED_COLOR`.
const FILE = '/file.css';
// A second file to inject. This sets the color to `INJECTED_COLOR2`.
const FILE2 = '/file2.css';

// Aliases for brevity.
const insertCSS = chrome.scripting.insertCSS;
const removeCSS = chrome.scripting.removeCSS;

// Returns the frame IDs from the tab with the given `tabId`.
async function getFrameIds(tabId) {
  // TODO(devlin: Promise-ify webNavigation.
  let frames = await new Promise(resolve => {
      chrome.webNavigation.getAllFrames({tabId}, resolve);
  });

  // Sort frames by frameId.
  let sortedFrames = frames.sort(
      (a, b) => a.frameId < b.frameId ? -1 : a.frameId > b.frameId ? 1 : 0);

  // Validate the frames - there should be 5 total, and the main frame should
  // be first.
  chrome.test.assertEq(5, sortedFrames.length);
  chrome.test.assertEq(sortedFrames[0].frameId, 0 /*main frame id*/);
  chrome.test.assertTrue(
      sortedFrames[1].frameId > 0 /* first non-main-frame id */);

  // Return the array of frame IDs.
  return sortedFrames.map(frame => frame.frameId);
}

// Returns the current color of the frame with `frameId` in the tab with
// `tabId`.
async function getCurrentColor(tabId, frameId) {
  const scriptResults = await chrome.scripting.executeScript({
    target: {tabId, frameIds: [frameId]},
    func: () => {
      const element = document.getElementById('main');
      const style = getComputedStyle(element);
      return style.getPropertyValue('color');
    },
  });
  chrome.test.assertEq(1, scriptResults.length)
  return scriptResults[0].result;
}

let tabId = -1;
let frameIds = [];

// `expectedColorsForFrames` holds a snapshot of expected values of the CSS
// colors being inserted/removed, for each of the frame ids. The array is
// sorted in the order of the frame IDs, such that the color at
// `expectedColorsForFrames[0] corresponds to the color for the frame with
// id `frameIds[0]`. Values get validated at the completion of the each test
// below.
//
// Each frame is a child of the frame preceding it. Frames 0 through 3 are
// <iframe src="..."> while frame 4 is <iframe srcdoc="..."> (about:srcdoc).
let expectedColorsForFrames = [
  ORIGINAL_COLOR, ORIGINAL_COLOR, ORIGINAL_COLOR, ORIGINAL_COLOR,
  ORIGINAL_COLOR
];

// Updates the expected state in `expectedColorsForFrames`. Undefined values in
// `delta` correspond to "no change" in `expectedColorsForFrames`.
function updateExpectedState(delta) {
  Object.assign(expectedColorsForFrames, delta);
}

// Validates that the colors are as expected for each frame.
async function checkColors() {
  for (let i = 0; i < frameIds.length; ++i) {
    const frameId = frameIds[i];
    let color = await getCurrentColor(tabId, frameId);
    chrome.test.assertEq(
        expectedColorsForFrames[i], color,
        `Improper color value for frame: ${frameId}`);
  }
}

// Loads `url` in a new tab, waits for it to finish loading, and returns the
// tabId of the newly-created tab.
// TODO(crbug.com/40568208): Update this to use
// test_resources/tabs_util.js when extension service workers support
// modules.
async function createTab(url) {
  return new Promise(resolve => {
    // Wait for `url` to finish loading.
    chrome.tabs.onUpdated.addListener(
        async function listener(updatedTabId, {status}) {
      if (status != 'complete')
        return;
      chrome.tabs.onUpdated.removeListener(listener);

      resolve(updatedTabId);
    });

    chrome.tabs.create({url});
  });
}

chrome.test.runTests([
  async function loadTab() {
    const config = await new Promise(resolve => {
      chrome.test.getConfig(resolve);
    });
    const testUrl = `http://example.com:${config.testServer.port}` +
        '/extensions/api_test/scripting/remove_css/test.html';
    tabId = await createTab(testUrl);
    frameIds = await getFrameIds(tabId);

    chrome.test.succeed();
  },
  async function insertCSSShouldSucceed() {
    // Insert CSS into every frame.
    await insertCSS({target: {tabId, allFrames: true}, css: CSS});
    updateExpectedState(
        [INJECTED_COLOR, INJECTED_COLOR, INJECTED_COLOR,
         INJECTED_COLOR, INJECTED_COLOR]);
    await checkColors();
    chrome.test.succeed();
  },
  async function removeCSSShouldSucceed() {
    // When no frame ID is specified, the CSS is removed from the top frame
    // Others should be unaffected (and keep the injected color).
    await removeCSS({target: {tabId}, css: CSS});
    updateExpectedState([ORIGINAL_COLOR, , , , , ]);
    await checkColors();
    chrome.test.succeed();
  },
  async function removeCSSWithDifferentCodeShouldDoNothing() {
    // If the specified code differs by even one character, it does not
    // match any inserted CSS and therefore nothing is removed.
    const slightlyOffCSS = CSS + ' ';
    // TODO(devlin): We don't currently return an error if the CSS to remove
    // did not match an inserted stylesheet. We could, which would make it
    // easier for developers to catch mistakes.
    await removeCSS({target: {tabId, allFrames: true}, css: slightlyOffCSS});
    // Note: no change in expected state.
    await checkColors();
    chrome.test.succeed();
  },
  async function removeCSSWithDifferentCSSOriginShouldDoNothing() {
    // If only the CSS origin differs, nothing is removed.
    await removeCSS({target: {tabId, frameIds}, css: CSS, origin: 'USER'});
    await checkColors();
    chrome.test.succeed();
  },
  async function removeCSSWithFrameIdShouldSucceed() {
    // When a frame ID is specified, the CSS is removed only from the given
    // frame.
    await removeCSS(
        {target: {tabId, frameIds: [frameIds[1]]}, css: CSS});
    updateExpectedState([ , ORIGINAL_COLOR, , , , ]);
    await checkColors();
    chrome.test.succeed();
  },
  async function removeCSSWithAllFramesShouldSucceed() {
    // When "allFrames" is set to true, the CSS is removed from all
    // frames.
    await removeCSS({target: {tabId, allFrames: true}, css: CSS});
    updateExpectedState([ORIGINAL_COLOR, ORIGINAL_COLOR, ORIGINAL_COLOR,
                         ORIGINAL_COLOR, ORIGINAL_COLOR]);
    await checkColors();
    chrome.test.succeed();
  },
  async function insertCSSWithFileShouldSucceed() {
    // Insert some CSS using a file (to then be removed).
    await insertCSS({target: {tabId, allFrames: true}, files: [FILE]});
    updateExpectedState([INJECTED_COLOR, INJECTED_COLOR, INJECTED_COLOR,
                         INJECTED_COLOR, INJECTED_COLOR]);
    await checkColors();
    chrome.test.succeed();
  },
  async function removeCSSWithFileShouldSucceed() {
    // When no frame ID is specified, the CSS is removed from the top frame.
    await removeCSS({target: {tabId}, files: [FILE]});
    updateExpectedState([ORIGINAL_COLOR, , , , , ]);
    await checkColors();
    chrome.test.succeed();
  },
  async function removeCSSWithDifferentFileShouldDoNothing() {
    // The CSS is not removed when passing a different file (even though the
    // contents of /file.css and /other.css are identical).
    await removeCSS(
        {target: {tabId, allFrames: true}, files: ['/other.css']});
    await checkColors();
    chrome.test.succeed();
  },
  async function insertAndRemoveCSSWithMultipleFilesShouldSucceed() {
    // Insert two style sheets. The second should "win", since it's latest
    // injected.
    await insertCSS({target: {tabId}, files: [FILE, FILE2]});
    updateExpectedState([INJECTED_COLOR2, , , , , ]);
    await checkColors();

    // Remove both previously-injected files.
    await removeCSS({target: {tabId}, files: [FILE, FILE2]});
    updateExpectedState([ORIGINAL_COLOR, , , , , ]);
    await checkColors();

    chrome.test.succeed();
  },
  async function insertMultipleFilesAndRemoveOneAtATime() {
    // Insert two style sheets. The second should "win", since it's latest
    // injected.
    await insertCSS({target: {tabId}, files: [FILE, FILE2]});
    updateExpectedState([INJECTED_COLOR2, , , , , ]);
    await checkColors();

    // Remove only one of the previously-injected files.
    await removeCSS({target: {tabId}, files: [FILE2]});
    updateExpectedState([INJECTED_COLOR, , , , , ]);
    await checkColors();

    // Now, remove the second.
    await removeCSS({target: {tabId}, files: [FILE]});
    updateExpectedState([ORIGINAL_COLOR, , , , , ]);
    await checkColors();

    chrome.test.succeed();
  },
  async function insertCSSWithDuplicateCodeShouldSucceed() {
    // Start by inserting the second CSS (which is a different color) into the
    // top frame.
    await insertCSS({target: {tabId}, css: CSS2});
    updateExpectedState([INJECTED_COLOR2, , , , , ]);
    await checkColors();
    // Then, re-insert the first CSS. The top frame should be updated.
    await insertCSS({target: {tabId}, css: CSS});
    updateExpectedState([INJECTED_COLOR, , , , , ]);
    await checkColors();
    chrome.test.succeed();
  },
  async function removeCSSWithDuplicateCodeShouldSucceed() {
    // Remove the first CSS. The second-inserted CSS should take effect again.
    await removeCSS({target: {tabId}, css: CSS});
    updateExpectedState([INJECTED_COLOR2, , , , , ]);
    await checkColors();

    // Remove the second CSS. The color should go back to the original color of
    // the frame.
    await removeCSS({target: {tabId}, css: CSS2});
    updateExpectedState([ORIGINAL_COLOR, , , , , ]);
    await checkColors();
    chrome.test.succeed();
  },
  async function noSuchTab() {
    const nonExistentTabId = 99999;
    await chrome.test.assertPromiseRejects(
        removeCSS({
          target: {
            tabId: nonExistentTabId,
          },
          css: CSS,
        }),
        `Error: No tab with id: ${nonExistentTabId}`);
    await checkColors();
    chrome.test.succeed();
  },
  async function noSuchFile() {
    const noSuchFile = 'no_such_file.js';
    // Edge case: When removing inserted files, we don't actually read
    // the file content (because it's unnecessary, and would be wasteful).
    // We also don't fire an error when there was no matching CSS inserted
    // (see "removeCSSWithDifferentCodeShouldDoNothing()" test case). This
    // combines to mean that even though there's no such file here, we
    // don't actually fire an error. This will be fixed if/when we return
    // an error for removing a non-existent stylesheet.
    await removeCSS({
      target: {tabId},
      files: [noSuchFile],
    });
    await checkColors();
    chrome.test.succeed();
  },
  async function disallowedPermission() {
    const config = await new Promise(resolve => {
      chrome.test.getConfig(resolve);
    });
    const testUrl = `http://google.com:${config.testServer.port}` +
        '/extensions/api_test/scripting/remove_css/test.html';
    tabId = await createTab(testUrl);
    // Note: We don't test the expected colors here, because that relies on the
    // extension having access to the host (in order to inject the script to
    // retrieve the colors), which it doesn't have here.
    await chrome.test.assertPromiseRejects(
        removeCSS({
          target: {tabId},
          css: CSS,
        }),
        'Error: Cannot access contents of the page. ' +
            'Extension manifest must request permission to ' +
            'access the respective host.');
    chrome.test.succeed();
  },
]);
