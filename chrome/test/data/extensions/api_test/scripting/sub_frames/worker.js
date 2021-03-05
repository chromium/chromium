// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function injectedFunction() {
  return location.href;
}

// Returns the error message used when the extension cannot access the contents
// of a frame.
function getAccessError(url) {
  return `Error: Cannot access contents of url "${url}". ` +
      'Extension manifest must request permission ' +
      'to access this host.';
}

// Returns the single tab matching the given `query`.
async function getSingleTab(query) {
  const tabs = await chrome.tabs.query(query);
  chrome.test.assertEq(1, tabs.length);
  return tabs[0];
}

// Returns all frames in the given tab.
async function getFramesInTab(tabId) {
  // TODO(devlin): Update this when webNavigation supports promises directly.
  const frames = await new Promise(resolve => {
    chrome.webNavigation.getAllFrames({tabId: tabId}, resolve);
  });
  chrome.test.assertTrue(frames.length > 0);
  return frames;
}

// Returns the ID of the frame with the given `hostname`.
function findFrameIdWithHostname(frames, hostname) {
  const frame = frames.find(frame => {
    return (new URL(frame.url)).hostname == hostname;
  });
  chrome.test.assertTrue(!!frame, 'No frame with hostname: ' + hostname);
  return frame.frameId;
}

chrome.test.runTests([
  async function allowedTopFrameAccess() {
    const query = {url: 'http://a.com/*'};
    let tab = await getSingleTab(query);
    const results = await chrome.scripting.executeScript({
      target: {
        tabId: tab.id,
        allFrames: true,
      },
      function: injectedFunction,
    });
    chrome.test.assertEq(2, results.length);

    // Note: The 'a.com' result is guaranteed to be first, since it's the root
    // frame.
    const url1 = new URL(results[0].result);
    chrome.test.assertEq('a.com', url1.hostname);
    chrome.test.assertEq(0, results[0].frameId);

    const url2 = new URL(results[1].result);
    chrome.test.assertEq('b.com', url2.hostname);
    // Verify the subframe has any non-main-frame ID. Note: specific frame IDs
    // are exercised more heavily below.
    chrome.test.assertFalse(results[1].frameId == 0);
    chrome.test.succeed();
  },

  async function disallowedTopFrameAccess() {
    const query = {url: 'http://d.com/*'};
    let tab = await getSingleTab(query);
    await chrome.test.assertPromiseRejects(
        chrome.scripting.executeScript({
          target: {
            tabId: tab.id,
            allFrames: true,
          },
          function: injectedFunction,
        }),
        getAccessError(tab.url));
    chrome.test.succeed();
  },

  // Tests injecting into a single specified frame.
  async function singleSpecificFrames() {
    const query = {url: 'http://a.com/*'};
    const tab = await getSingleTab(query);
    const frames = await getFramesInTab(tab.id);
    const frameId = findFrameIdWithHostname(frames, 'b.com');

    const results = await chrome.scripting.executeScript({
      target: {
        tabId: tab.id,
        frameIds: [frameId],
      },
      function: injectedFunction,
    });
    chrome.test.assertEq(1, results.length);

    const resultUrl = new URL(results[0].result);
    chrome.test.assertEq('b.com', resultUrl.hostname);
    chrome.test.assertEq(frameId, results[0].frameId);
    chrome.test.succeed();
  },

  // Tests injecting when multiple frames are specified.
  async function multipleSpecificFrames() {
    const query = {url: 'http://a.com/*'};
    const tab = await getSingleTab(query);
    const frames = await getFramesInTab(tab.id);
    const frameIds = [
        findFrameIdWithHostname(frames, 'a.com'),
        findFrameIdWithHostname(frames, 'b.com'),
    ];

    const results = await chrome.scripting.executeScript({
      target: {
        tabId: tab.id,
        frameIds: frameIds,
      },
      function: injectedFunction,
    });
    chrome.test.assertEq(2, results.length);

    // Since we specified frame IDs, there's no guarantee as to the order
    // of the result. Compare a sorted output.
    const resultUrls = results.map(result => {
      return (new URL(result.result)).hostname;
    });
    chrome.test.assertEq(['a.com', 'b.com'], resultUrls.sort());
    chrome.test.assertEq(
        frameIds,
        results.map(result => result.frameId).sort());
    chrome.test.succeed();
  },

  // Tests injecting with duplicate frame IDs specified.
  async function duplicateSpecificFrames() {
    const query = {url: 'http://a.com/*'};
    const tab = await getSingleTab(query);
    const frames = await getFramesInTab(tab.id);
    const frameId = findFrameIdWithHostname(frames, 'b.com');

    const results = await chrome.scripting.executeScript({
      target: {
        tabId: tab.id,
        frameIds: [frameId, frameId],
      },
      function: injectedFunction,
    });
    chrome.test.assertEq(1, results.length);

    const resultUrl = new URL(results[0].result);
    chrome.test.assertEq('b.com', resultUrl.hostname);
    chrome.test.assertEq(frameId, results[0].frameId);
    chrome.test.succeed();
  },

  // Tests that an error is thrown when an extension doesn't have access to
  // one of the frames specified.
  async function disallowedSpecificFrame() {
    const query = {url: 'http://a.com/*'};
    const tab = await getSingleTab(query);
    const frames = await getFramesInTab(tab.id);
    const deniedFrame = frames.find((frame) => {
      return (new URL(frame.url)).hostname == 'c.com';
    });
    const frameIds = [
        findFrameIdWithHostname(frames, 'b.com'),
        findFrameIdWithHostname(frames, 'c.com'),
    ];

    await chrome.test.assertPromiseRejects(
        chrome.scripting.executeScript({
          target: {
            tabId: tab.id,
            frameIds: frameIds,
          },
          function: injectedFunction,
        }),
        getAccessError(deniedFrame.url));
    chrome.test.succeed();
  },

  // Tests that an error is thrown when specifying a non-existent frame ID.
  async function nonExistentSpecificFrame() {
    const query = {url: 'http://a.com/*'};
    const tab = await getSingleTab(query);
    const frames = await getFramesInTab(tab.id);
    const nonExistentFrameId = 99999;
    const frameIds = [
        findFrameIdWithHostname(frames, 'b.com'),
        nonExistentFrameId,
    ];

    await chrome.test.assertPromiseRejects(
        chrome.scripting.executeScript({
          target: {
            tabId: tab.id,
            frameIds: frameIds,
          },
          function: injectedFunction,
        }),
        `Error: No frame with id ${nonExistentFrameId} in ` +
            `tab with id ${tab.id}`);
    chrome.test.succeed();
  },

  // Test that an extension cannot specify both allFrames and frameIds.
  async function specifyingBothFrameIdsAndAllFramesIsInvalid() {
    const query = {url: 'http://a.com/*'};
    const tab = await getSingleTab(query);
    const frames = await getFramesInTab(tab.id);
    const frameIds = [
        findFrameIdWithHostname(frames, 'b.com'),
    ];

    await chrome.test.assertPromiseRejects(
        chrome.scripting.executeScript({
          target: {
            tabId: tab.id,
            frameIds: frameIds,
            allFrames: true,
          },
          function: injectedFunction,
        }),
        `Error: Cannot specify both 'allFrames' and 'frameIds'.`);
    chrome.test.succeed();
  },
]);
