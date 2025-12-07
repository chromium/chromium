// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {findDocumentIdWithHostname, findFrameIdWithHostname, findFrameWithHostname, getFramesInTab, getSingleTab} from '/_test_resources/test_util/tabs_util.js';
import {waitForUserScriptsAPIAllowed} from '/_test_resources/test_util/user_script_test_util.js';

const locationScript = `(function() { return location.href })()`;

chrome.test.runTests([
  waitForUserScriptsAPIAllowed,

  // Tests injecting a script when the extension has site access to the top
  // frame (a.com).
  async function allowedTopFrameAccess() {
    await chrome.userScripts.unregister();

    const query = {url: 'http://a.com/*'};
    let tab = await getSingleTab(query);

    const script = {
      js: [{code: locationScript}],
      target: {allFrames: true, tabId: tab.id}
    };
    const results = await chrome.userScripts.execute(script);
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

  // Tests injecting a script when the extension doesn't have site access to the
  // top frame (d.com).
  async function disallowedTopFrameAccess() {
    await chrome.userScripts.unregister();

    const query = {url: 'http://d.com/*'};
    let tab = await getSingleTab(query);

    const script = {
      js: [{code: locationScript}],
      target: {allFrames: true, tabId: tab.id}
    };
    await chrome.test.assertPromiseRejects(
        chrome.userScripts.execute(script),
        `Error: Cannot access contents of url "${tab.url}". Extension ` +
            'manifest must request permission to access this host.');

    chrome.test.succeed();
  },

  // Tests injecting a script into a single frame id.
  async function singleFrame_FrameId() {
    await chrome.userScripts.unregister();

    const query = {url: 'http://a.com/*'};
    const tab = await getSingleTab(query);
    const frames = await getFramesInTab(tab.id);
    const frame = findFrameWithHostname(frames, 'b.com');

    const script = {
      js: [{code: locationScript}],
      target: {frameIds: [frame.frameId], tabId: tab.id}
    };
    const results = await chrome.userScripts.execute(script);

    // Verify script is injected on the specific frame id.
    chrome.test.assertEq(1, results.length);
    const resultUrl = new URL(results[0].result);
    chrome.test.assertEq('b.com', resultUrl.hostname);
    chrome.test.assertEq(frame.frameId, results[0].frameId);

    chrome.test.succeed();
  },

  // Tests injecting a script into a single document id.
  async function singleFrame_DocumentId() {
    await chrome.userScripts.unregister();

    const query = {url: 'http://a.com/*'};
    const tab = await getSingleTab(query);
    const frames = await getFramesInTab(tab.id);
    const frame = findFrameWithHostname(frames, 'b.com');

    const script = {
      js: [{code: locationScript}],
      target: {documentIds: [frame.documentId], tabId: tab.id}
    };
    const results = await chrome.userScripts.execute(script);

    // Verify script is injected in the specific document id.
    chrome.test.assertEq(1, results.length);
    const resultUrl = new URL(results[0].result);
    chrome.test.assertEq('b.com', resultUrl.hostname);
    chrome.test.assertEq(frame.documentId, results[0].documentId);

    chrome.test.succeed();
  },

  // Tests injecting a script into multiple frame ids.
  async function multipleFrames_frameIds() {
    await chrome.userScripts.unregister();

    const query = {url: 'http://a.com/*'};
    const tab = await getSingleTab(query);
    const frames = await getFramesInTab(tab.id);
    const frameIds = [
      findFrameIdWithHostname(frames, 'a.com'),
      findFrameIdWithHostname(frames, 'b.com'),
    ];

    const script = {
      js: [{code: locationScript}],
      target: {frameIds: frameIds, tabId: tab.id}
    };
    const results = await chrome.userScripts.execute(script);

    // Verify script is injected in both frame ids. Note that the order of the
    // result is not guaranteed when injecting to specific frames.
    chrome.test.assertEq(2, results.length);
    chrome.test.checkDeepEq(['a.com', 'b.com'], results.map(result => {
      return (new URL(result.result)).hostname;
    }));
    chrome.test.checkDeepEq(frameIds, results.map(result => result.frameId));

    chrome.test.succeed();
  },

  // Tests injecting a script into multiple document ids.
  async function multipleFrames_documentIds() {
    await chrome.userScripts.unregister();

    const query = {url: 'http://a.com/*'};
    const tab = await getSingleTab(query);
    const frames = await getFramesInTab(tab.id);
    const documentIds = [
      findDocumentIdWithHostname(frames, 'a.com'),
      findDocumentIdWithHostname(frames, 'b.com'),
    ];

    const script = {
      js: [{code: locationScript}],
      target: {documentIds: documentIds, tabId: tab.id}
    };
    const results = await chrome.userScripts.execute(script);

    // Verify script is injected in both document ids. Note that the order of
    // the result is not guaranteed when injecting to specific frames.
    chrome.test.assertEq(2, results.length);
    chrome.test.checkDeepEq(['a.com', 'b.com'], results.map(result => {
      return (new URL(result.result)).hostname;
    }));
    chrome.test.checkDeepEq(
        documentIds, results.map(result => result.documentId));

    chrome.test.succeed();
  },

  // Tests injecting a script with duplicate frame ids specified.
  async function duplicateFrames_frameIds() {
    await chrome.userScripts.unregister();

    const query = {url: 'http://a.com/*'};
    const tab = await getSingleTab(query);
    const frames = await getFramesInTab(tab.id);
    const frame = findFrameWithHostname(frames, 'b.com');

    const script = {
      js: [{code: locationScript}],
      target: {frameIds: [frame.frameId, frame.frameId], tabId: tab.id}
    };
    const results = await chrome.userScripts.execute(script);

    // Verify script is only injected once to the duplicated frame id.
    chrome.test.assertEq(1, results.length);
    let resultUrl = new URL(results[0].result);
    chrome.test.assertEq('b.com', resultUrl.hostname);
    chrome.test.assertEq(frame.frameId, results[0].frameId);

    chrome.test.succeed();
  },

  // Tests injecting a script with duplicate frame ids specified.
  async function duplicateFrames_documentIds() {
    await chrome.userScripts.unregister();

    const query = {url: 'http://a.com/*'};
    const tab = await getSingleTab(query);
    const frames = await getFramesInTab(tab.id);
    const frame = findFrameWithHostname(frames, 'b.com');

    const script = {
      js: [{code: locationScript}],
      target: {documentIds: [frame.documentId, frame.documentId], tabId: tab.id}
    };
    const results = await chrome.userScripts.execute(script);

    // Verify script is only injected once to the duplicated frame id.
    chrome.test.assertEq(1, results.length);
    let resultUrl = new URL(results[0].result);
    chrome.test.assertEq('b.com', resultUrl.hostname);
    chrome.test.assertEq(frame.documentId, results[0].documentId);

    chrome.test.succeed();
  },

  // Tests that an error is thrown when an extension doesn't have access to one
  // of the frames specified
  async function disallowedFrame_frameId() {
    await chrome.userScripts.unregister();

    const query = {url: 'http://a.com/*'};
    const tab = await getSingleTab(query);
    const frames = await getFramesInTab(tab.id);
    const frameIds = [
      findFrameIdWithHostname(frames, 'b.com'),
      findFrameIdWithHostname(frames, 'c.com'),
    ];
    const deniedFrame = frames.find((frame) => {
      return (new URL(frame.url)).hostname == 'c.com';
    });

    const script = {
      js: [{code: locationScript}],
      target: {frameIds: frameIds, tabId: tab.id}
    };
    await chrome.test.assertPromiseRejects(
        chrome.userScripts.execute(script),
        `Error: Cannot access contents of url "${
            deniedFrame.url}". Extension ` +
            'manifest must request permission to access this host.');

    chrome.test.succeed();
  },

  // Tests that an error is thrown when an extension doesn't have access to one
  // of the documents specified
  async function disallowedFrame_frameId() {
    await chrome.userScripts.unregister();

    const query = {url: 'http://a.com/*'};
    const tab = await getSingleTab(query);
    const frames = await getFramesInTab(tab.id);
    const documentIds = [
      findDocumentIdWithHostname(frames, 'b.com'),
      findDocumentIdWithHostname(frames, 'c.com'),
    ];
    const deniedFrame = frames.find((frame) => {
      return (new URL(frame.url)).hostname == 'c.com';
    });

    const script = {
      js: [{code: locationScript}],
      target: {documentIds: documentIds, tabId: tab.id}
    };
    await chrome.test.assertPromiseRejects(
        chrome.userScripts.execute(script),
        `Error: Cannot access contents of url "${
            deniedFrame.url}". Extension ` +
            'manifest must request permission to access this host.');

    chrome.test.succeed();
  },
])
