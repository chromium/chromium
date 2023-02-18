// Copyright 2020 The Chromium Authors
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
  const frames = await chrome.webNavigation.getAllFrames({tabId: tabId});
  chrome.test.assertTrue(frames.length > 0);
  return frames;
}

// Returns the frame with the given `hostname`.
function findFrameWithHostname(frames, hostname) {
  const frame = frames.find(frame => {
    return (new URL(frame.url)).hostname == hostname;
  });
  chrome.test.assertTrue(!!frame, 'No frame with hostname: ' + hostname);
  return frame;
}

// Returns the ID of the frame with the given `hostname`.
function findFrameIdWithHostname(frames, hostname) {
  return findFrameWithHostname(frames, hostname).frameId;
}

// Returns the ID of the document with the given `hostname`.
function findDocumentIdWithHostname(frames, hostname) {
  return findFrameWithHostname(frames, hostname).documentId;
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
      func: injectedFunction,
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
    chrome.test.assertNe(0, results[1].frameId);
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
          func: injectedFunction,
        }),
        getAccessError(tab.url));
    chrome.test.succeed();
  },

  // Tests injecting into a single specified frame.
  async function singleSpecificFrames() {
    const query = {url: 'http://a.com/*'};
    const tab = await getSingleTab(query);
    const frames = await getFramesInTab(tab.id);
    const frame = findFrameWithHostname(frames, 'b.com');

    let results = await chrome.scripting.executeScript({
      target: {
        tabId: tab.id,
        frameIds: [frame.frameId],
      },
      func: injectedFunction,
    });
    chrome.test.assertEq(1, results.length);

    let resultUrl = new URL(results[0].result);
    chrome.test.assertEq('b.com', resultUrl.hostname);
    chrome.test.assertEq(frame.frameId, results[0].frameId);

    // Now try via documentId.
    results = await chrome.scripting.executeScript({
      target: {
        tabId: tab.id,
        documentIds: [frame.documentId],
      },
      func: injectedFunction,
    });
    chrome.test.assertEq(1, results.length);

    resultUrl = new URL(results[0].result);
    chrome.test.assertEq('b.com', resultUrl.hostname);
    chrome.test.assertEq(frame.documentId, results[0].documentId);
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
    const documentIds = [
        findDocumentIdWithHostname(frames, 'a.com'),
        findDocumentIdWithHostname(frames, 'b.com'),
    ];

    let results = await chrome.scripting.executeScript({
      target: {
        tabId: tab.id,
        frameIds: frameIds,
      },
      func: injectedFunction,
    });
    chrome.test.assertEq(2, results.length);

    // Since we specified frame IDs, there's no guarantee as to the order
    // of the result. Compare a sorted output.
    let resultUrls = results.map(result => {
      return (new URL(result.result)).hostname;
    });
    chrome.test.assertEq(['a.com', 'b.com'], resultUrls.sort());
    chrome.test.assertEq(
        frameIds,
        results.map(result => result.frameId).sort());

    // Now try the via documentId.
    results = await chrome.scripting.executeScript({
      target: {
        tabId: tab.id,
        documentIds: documentIds,
      },
      func: injectedFunction,
    });
    chrome.test.assertEq(2, results.length);

    // Since we specified frame IDs, there's no guarantee as to the order
    // of the result. Compare a sorted output.
    resultUrls = results.map(result => {
      return (new URL(result.result)).hostname;
    });
    chrome.test.assertEq(['a.com', 'b.com'], resultUrls.sort());
    chrome.test.assertEq(
        documentIds.sort(),
        results.map(result => result.documentId).sort());

    chrome.test.succeed();
  },

  // Tests injecting with duplicate frame IDs specified.
  async function duplicateSpecificFrames() {
    const query = {url: 'http://a.com/*'};
    const tab = await getSingleTab(query);
    const frames = await getFramesInTab(tab.id);
    const frame = findFrameWithHostname(frames, 'b.com');

    let results = await chrome.scripting.executeScript({
      target: {
        tabId: tab.id,
        frameIds: [frame.frameId, frame.frameId],
      },
      func: injectedFunction,
    });
    chrome.test.assertEq(1, results.length);

    let resultUrl = new URL(results[0].result);
    chrome.test.assertEq('b.com', resultUrl.hostname);
    chrome.test.assertEq(frame.frameId, results[0].frameId);

    // Now try the via documentId.
    results = await chrome.scripting.executeScript({
      target: {
        tabId: tab.id,
        documentIds: [frame.documentId, frame.documentId],
      },
      func: injectedFunction,
    });
    chrome.test.assertEq(1, results.length);

    resultUrl = new URL(results[0].result);
    chrome.test.assertEq('b.com', resultUrl.hostname);
    chrome.test.assertEq(frame.documentId, results[0].documentId);
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
    const documentIds = [
        findDocumentIdWithHostname(frames, 'b.com'),
        findDocumentIdWithHostname(frames, 'c.com'),
    ];

    await chrome.test.assertPromiseRejects(
        chrome.scripting.executeScript({
          target: {
            tabId: tab.id,
            frameIds: frameIds,
          },
          func: injectedFunction,
        }),
        getAccessError(deniedFrame.url));

    // Now try the via documentId.
    await chrome.test.assertPromiseRejects(
        chrome.scripting.executeScript({
          target: {
            tabId: tab.id,
            documentIds: documentIds,
          },
          func: injectedFunction,
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
    const nonExistentDocumentId = '0123456789ABCDEF0123456789ABCDEF';
    const frameIds = [
        findFrameIdWithHostname(frames, 'b.com'),
        nonExistentFrameId,
    ];
    const documentIds = [
        findDocumentIdWithHostname(frames, 'b.com'),
        nonExistentDocumentId,
    ];

    await chrome.test.assertPromiseRejects(
        chrome.scripting.executeScript({
          target: {
            tabId: tab.id,
            frameIds: frameIds,
          },
          func: injectedFunction,
        }),
        `Error: No frame with id ${nonExistentFrameId} in ` +
            `tab with id ${tab.id}`);

    // Now try the via documentId.
    await chrome.test.assertPromiseRejects(
        chrome.scripting.executeScript({
          target: {
            tabId: tab.id,
            documentIds: documentIds,
          },
          func: injectedFunction,
        }),
        `Error: No document with id ${nonExistentDocumentId} in ` +
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
    const documentIds = [
        findDocumentIdWithHostname(frames, 'b.com'),
    ];

    await chrome.test.assertPromiseRejects(
        chrome.scripting.executeScript({
          target: {
            tabId: tab.id,
            frameIds: frameIds,
            allFrames: true,
          },
          func: injectedFunction,
        }),
        `Error: Cannot specify 'allFrames' if either 'frameIds' or ` +
            `'documentIds' is specified.`);

    // Now try the via documentId.
    await chrome.test.assertPromiseRejects(
        chrome.scripting.executeScript({
          target: {
            tabId: tab.id,
            documentIds: documentIds,
            allFrames: true,
          },
          func: injectedFunction,
        }),
        `Error: Cannot specify 'allFrames' if either 'frameIds' or ` +
            `'documentIds' is specified.`);
    chrome.test.succeed();
  },

  // Test that an extension cannot specify both frameIds and documentIds.
  async function specifyingBothFrameIdsAndDocumentIdsIsInvalid() {
    const query = {url: 'http://a.com/*'};
    const tab = await getSingleTab(query);
    const frames = await getFramesInTab(tab.id);
    const frameIds = [
        findFrameIdWithHostname(frames, 'b.com'),
    ];
    const documentIds = [
        findDocumentIdWithHostname(frames, 'b.com'),
    ];

    await chrome.test.assertPromiseRejects(
        chrome.scripting.executeScript({
          target: {
            tabId: tab.id,
            documentIds: documentIds,
            frameIds: frameIds
          },
          func: injectedFunction,
        }),
        `Error: Cannot specify both 'frameIds' and 'documentIds'.`);
    chrome.test.succeed();
  },

  // Test that an extension cannot specify both a documentId from another tab.
  async function specifyingBothFrameIdsAndDocumentIdsIsInvalid() {
    const query_a = {url: 'http://a.com/*'};
    const tab_a = await getSingleTab(query_a);
    const query_d = {url: 'http://d.com/*'};
    const tab_d = await getSingleTab(query_d);
    const frames = await getFramesInTab(tab_d.id);
    const documentIds = [
        findDocumentIdWithHostname(frames, 'b.com'),
    ];

    await chrome.test.assertPromiseRejects(
        chrome.scripting.executeScript({
          target: {
            tabId: tab_a.id,
            documentIds: documentIds
          },
          func: injectedFunction,
        }),
        `Error: No document with id ${documentIds[0]} in ` +
            `tab with id ${tab_a.id}`);
    chrome.test.succeed();
  },

]);
