// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Opens a new tab and waits for it to finish loading.
 * @param {string} url The url for the new tab to point to.
 * @return {!Promise<tabs.Tab>} A promise that resolves when the tab has
 *     finished loading, with the value of the tab.
 */
export function openTab(url) {
  return new Promise((resolve) => {
    let createdTabId;
    let completedTabIds = [];
    chrome.tabs.onUpdated.addListener(
        function listener(tabId, changeInfo, tab) {
      if (changeInfo.status !== 'complete')
        return;  // Tab not done.

      if (createdTabId === undefined) {
        // A tab completed loading before the chrome.tabs.create callback was
        // triggered; stash the ID for later comparison to see if it was our
        // tab.
        completedTabIds.push(tabId);
        return;
      }

      if (tabId !== createdTabId)
        return;  // Not our tab.

      // It's ours!
      chrome.tabs.onUpdated.removeListener(listener);
      resolve(tab);
    });
    chrome.tabs.create({url: url}, (tab) => {
      if (completedTabIds.includes(tab.id))
        resolve(tab);
      else
        createdTabId = tab.id;
    });
  });
}

/**
 *  Returns the single tab matching the given `query`.
 * @param {Object} query
 * @return {chrome.tabs.Tab}
 */
export async function getSingleTab(query) {
  const tabs = await chrome.tabs.query(query);
  chrome.test.assertEq(1, tabs.length);
  return tabs[0];
}

/**
 * Returns the injected element ids in `tabId` by alphabetical order.
 * @param {string} tabId
 * @return {string[]}
 */
export async function getInjectedElementIds(tabId) {
  let injectedElements = await chrome.scripting.executeScript({
    target: { tabId: tabId },
    func: () => {
      let childIds = [];
      for (const child of document.body.children)
        childIds.push(child.id);
      return childIds.sort();
    }
  });
  chrome.test.assertEq(1, injectedElements.length);
  return injectedElements[0].result;
};

/**
 * Returns the injected element ids in `tabId` by injection order.
 * @param {string} tabId
 * @return {string[]}
 */
export async function getInjectedElementIdsInOrder(tabId) {
  let injectedElements = await chrome.scripting.executeScript({
    target: {tabId: tabId},
    func: () => {
      let childIds = [];
      for (const child of document.body.children)
        childIds.push(child.id);
      return childIds;
    }
  });
  chrome.test.assertEq(1, injectedElements.length);
  return injectedElements[0].result;
};

/**
 * Returns the frames in the given tab.
 * @param {string} tabId
 * @return {string[]}
 */
export async function getFramesInTab(tabId) {
  const frames = await chrome.webNavigation.getAllFrames({tabId: tabId});
  chrome.test.assertTrue(frames.length > 0);
  return frames;
}

/**
 * Returns the frame with the given `hostname`.
 * @param {string} frames
 * @param {string} hostname
 * @return {string} frame
 */
export function findFrameWithHostname(frames, hostname) {
  const frame = frames.find(frame => {
    return (new URL(frame.url)).hostname == hostname;
  });
  chrome.test.assertTrue(!!frame, 'No frame with hostname: ' + hostname);
  return frame;
}

/**
 * Returns the ID of the frame with the given `hostname`.
 * @param {string} frames
 * @param {string} hostname
 * @return {string} frameId
 */
export function findFrameIdWithHostname(frames, hostname) {
  return findFrameWithHostname(frames, hostname).frameId;
}

/**
 * Returns the ID of the document with the given `hostname`.
 * @param {string} frames
 * @param {string} hostname
 * @return {string} frameId
 */
export function findDocumentIdWithHostname(frames, hostname) {
  return findFrameWithHostname(frames, hostname).documentId;
}
