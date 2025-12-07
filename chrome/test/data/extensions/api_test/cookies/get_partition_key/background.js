// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function getActiveTab() {
  return new Promise(function(resolve, reject) {
    chrome.tabs.query({active: true, currentWindow: true}, function(tabs) {
      if (tabs.length > 0) {
        resolve(tabs[0]);
      } else {
        reject('Active tab not found');
      }
    });
  });
}

// Gets frames and returns them in ascending order of id value.
function getFrames(id) {
  return new Promise(function(resolve, reject) {
    chrome.webNavigation.getAllFrames({tabId: id}, function(frames) {
      if (frames.length > 0) {
        resolve(frames.sort((a, b) => a.frameId - b.frameId));
      } else {
        reject('No frames found');
      }
    });
  });
}

let pass = chrome.test.callbackPass;
let fail = chrome.test.callbackFail;

let tab;
let topLevelFrame;
let crossSiteFrame;
let noHostPermissionsFrame;

chrome.test.runTests([
  async function confirmExtensionSetup() {
    // Confirm that tab is set up correctly by initialization in test fixture.
    // The active tab should have a top-level site (a.com) which embeds two
    // cross-site frames one with host permissions (b.com) and the other (c.com)
    // without.
    tab = await getActiveTab();
    chrome.test.assertNe(tab, null);

    const frames = await getFrames(tab.id);
    chrome.test.assertEq(frames.length, 3);

    // TopLevelFrame is guaranteed to be first frame since it's id will be 0,
    // but subsequent frames are not guaranteed to be in order.
    topLevelFrame = frames[0];
    if (frames[1].url.includes('b.com')) {
      crossSiteFrame = frames[1];
      noHostPermissionsFrame = frames[2];
    } else {
      crossSiteFrame = frames[2];
      noHostPermissionsFrame = frames[1];
    }

    chrome.test.assertTrue(topLevelFrame.url.includes('a.com'));
    chrome.test.assertTrue(crossSiteFrame.url.includes('b.com'));
    chrome.test.assertTrue(noHostPermissionsFrame.url.includes('c.com'));

    chrome.test.succeed();
  },
  async function checkInvalidInputs() {
    await chrome.test.assertPromiseRejects(
        chrome.cookies.getPartitionKey({tabId: (tab.id - 1), frameId: 0}),
        'Error: Invalid `tabId`.');

    await chrome.test.assertPromiseRejects(
        chrome.cookies.getPartitionKey({tabId: tab.id, frameId: -1}),
        'Error: Invalid `frameId`.');

    await chrome.test.assertPromiseRejects(
        chrome.cookies.getPartitionKey(
            {tabId: tab.id, frameId: 0, documentId: ''}),
        'Error: Invalid `documentId`.');

    chrome.test.succeed();
  },
  async function testHostPermissionsRequired() {
    const noHostPermissionsDocId = noHostPermissionsFrame.documentId;
    chrome.test.assertNe(noHostPermissionsDocId.length, 0);

    await chrome.test.assertPromiseRejects(
        chrome.cookies.getPartitionKey({documentId: noHostPermissionsDocId}),
        'Error: No host permissions for cookies at url: "' +
            noHostPermissionsFrame.url + '".')
    chrome.test.succeed();
  },
  async function testFameIdOnlyInput() {
    await chrome.test.assertPromiseRejects(
        chrome.cookies.getPartitionKey({frameId: 0}),
        'Error: `frameId` may not be 0 if no `tabId` is present.')

    const expectedCrossSiteKey = {
      'partitionKey':
          {'topLevelSite': 'http://a.com', 'hasCrossSiteAncestor': true}
    };

    let actualPartitionKey =
        await chrome.cookies.getPartitionKey({frameId: crossSiteFrame.frameId});
    chrome.test.assertEq(expectedCrossSiteKey, actualPartitionKey);
    chrome.test.succeed();
  },
  async function testTopLevelFrame() {
    const topLevelDocId = topLevelFrame.documentId;
    chrome.test.assertNe(topLevelDocId.length, 0);

    const expectedTopLevelKey = {
      'partitionKey':
          {'topLevelSite': 'http://a.com', 'hasCrossSiteAncestor': false}
    };

    let actualPartitionKey =
        await chrome.cookies.getPartitionKey({documentId: topLevelDocId});
    chrome.test.assertEq(expectedTopLevelKey, actualPartitionKey);

    actualPartitionKey = await chrome.cookies.getPartitionKey(
        {tabId: tab.id, frameId: topLevelFrame.frameId});
    chrome.test.assertEq(expectedTopLevelKey, actualPartitionKey);

    actualPartitionKey = await chrome.cookies.getPartitionKey({
      tabId: tab.id,
      frameId: topLevelFrame.frameId,
      documentId: topLevelDocId
    });
    chrome.test.assertEq(expectedTopLevelKey, actualPartitionKey);

    // Providing no frameId defaults to topLevel frameId (0).
    actualPartitionKey = await chrome.cookies.getPartitionKey({
      tabId: tab.id,
    });
    chrome.test.assertEq(expectedTopLevelKey, actualPartitionKey);
    chrome.test.succeed();
  },
  async function testCrossSiteFrame() {
    const crossSiteDocId = crossSiteFrame.documentId;
    chrome.test.assertNe(crossSiteDocId.length, 0);

    const expectedCrossSiteKey = {
      'partitionKey':
          {'topLevelSite': 'http://a.com', 'hasCrossSiteAncestor': true}
    };

    let actualPartitionKey =
        await chrome.cookies.getPartitionKey({documentId: crossSiteDocId});
    chrome.test.assertEq(expectedCrossSiteKey, actualPartitionKey);

    actualPartitionKey = await chrome.cookies.getPartitionKey(
        {tabId: tab.id, frameId: crossSiteFrame.frameId});
    chrome.test.assertEq(expectedCrossSiteKey, actualPartitionKey);

    actualPartitionKey = await chrome.cookies.getPartitionKey({
      tabId: tab.id,
      frameId: crossSiteFrame.frameId,
      documentId: crossSiteDocId
    });
    chrome.test.assertEq(expectedCrossSiteKey, actualPartitionKey);

    chrome.test.succeed();
  },
]);
