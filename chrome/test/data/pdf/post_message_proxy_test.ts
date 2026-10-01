// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PdfViewerElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

const viewer = document.body.querySelector<PdfViewerElement>('#viewer')!;

const tests = [
  async function testNoToken() {
    const whenConnectionDenied =
        eventToPromise('connection-denied-for-testing', viewer);
    window.postMessage({type: 'connect'});
    await whenConnectionDenied;
    chrome.test.succeed();
  },
  async function testBadToken() {
    const whenConnectionDenied =
        eventToPromise('connection-denied-for-testing', viewer);
    window.postMessage({type: 'connect', token: 'foo'});
    await whenConnectionDenied;
    chrome.test.succeed();
  },
  function testCrossOriginReplyTargetsEmbedderOrigin() {
    // Replies to a cross-origin embedder must be addressed to the origin that
    // embedder had when it contacted the viewer, never broadcast with a
    // wildcard target origin. 'documentLoaded' and 'passwordPrompted' share
    // this code path; only 'documentLoaded' can be exercised here, because
    // 'passwordPrompted' is sent while the document is still loading, before
    // a parent window can be registered.
    const replies: Array<{type: string, targetOrigin: string}> = [];
    const mockEmbedder = {
      postMessage(message: {type: string}, targetOrigin: string) {
        replies.push({type: message.type, targetOrigin});
      },
    };

    // Registering a new parent window makes the viewer send 'documentLoaded',
    // since the document has already finished loading.
    viewer.handleScriptingMessage({
      data: {type: 'initialize'},
      source: mockEmbedder,
      origin: 'https://example.com',
    } as unknown as MessageEvent);

    chrome.test.assertEq(1, replies.length);
    chrome.test.assertEq('documentLoaded', replies[0]!.type);
    chrome.test.assertEq('https://example.com', replies[0]!.targetOrigin);
    chrome.test.succeed();
  },
  function testOpaqueOriginEmbedderReplyUsesWildcard() {
    // An embedder with an opaque origin, such as one loaded from a data: or
    // file: URL, reports its origin as 'null'. That cannot be named as a
    // target origin, so these messages have to fall back to the wildcard,
    // otherwise the embedder never learns that the document loaded.
    const replies: Array<{type: string, targetOrigin: string}> = [];
    const mockEmbedder = {
      postMessage(message: {type: string}, targetOrigin: string) {
        replies.push({type: message.type, targetOrigin});
      },
    };

    viewer.handleScriptingMessage({
      data: {type: 'initialize'},
      source: mockEmbedder,
      origin: 'null',
    } as unknown as MessageEvent);

    chrome.test.assertEq(1, replies.length);
    chrome.test.assertEq('documentLoaded', replies[0]!.type);
    chrome.test.assertEq('*', replies[0]!.targetOrigin);
    chrome.test.succeed();
  },
];

chrome.test.runTests(tests);
