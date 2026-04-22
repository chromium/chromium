// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const SCRIPT_URL = '_test_resources/api_test/webnavigation/framework.js';
const loadScript = chrome.test.loadScript(SCRIPT_URL);

loadScript.then(async function() {
  const tab = await promise(chrome.tabs.create, {url: 'about:blank'});
  const config = await promise(chrome.test.getConfig);
  const port = config.testServer.port;

  const urlLoad = `http://127.0.0.1:${port}/` +
      'extensions/api_test/webnavigation/targetBlank/a.html';
  const urlTarget = `http://127.0.0.1:${port}/` +
      'extensions/api_test/webnavigation/targetBlank/b.html';

  chrome.test.runTests([
    // Opens a tab and waits for the user to click on a link with
    // target=_blank in it.
    function targetBlank() {
      expect(
          [
            {
              label: 'a-onBeforeNavigate',
              event: 'onBeforeNavigate',
              details: {
                documentLifecycle: 'active',
                frameId: 0,
                frameType: 'outermost_frame',
                parentFrameId: -1,
                processId: -1,
                tabId: 0,
                timeStamp: 0,
                url: urlLoad
              }
            },
            {
              label: 'a-onCommitted',
              event: 'onCommitted',
              details: {
                documentId: 1,
                documentLifecycle: 'active',
                frameId: 0,
                frameType: 'outermost_frame',
                parentFrameId: -1,
                processId: 0,
                tabId: 0,
                timeStamp: 0,
                transitionQualifiers: [],
                transitionType: 'link',
                url: urlLoad
              }
            },
            {
              label: 'a-onDOMContentLoaded',
              event: 'onDOMContentLoaded',
              details: {
                documentId: 1,
                documentLifecycle: 'active',
                frameId: 0,
                frameType: 'outermost_frame',
                parentFrameId: -1,
                processId: 0,
                tabId: 0,
                timeStamp: 0,
                url: urlLoad
              }
            },
            {
              label: 'a-onCompleted',
              event: 'onCompleted',
              details: {
                documentId: 1,
                documentLifecycle: 'active',
                frameId: 0,
                frameType: 'outermost_frame',
                parentFrameId: -1,
                processId: 0,
                tabId: 0,
                timeStamp: 0,
                url: urlLoad
              }
            },
            {
              label: 'b-onCreatedNavigationTarget',
              event: 'onCreatedNavigationTarget',
              details: {
                sourceFrameId: 0,
                sourceProcessId: 0,
                sourceTabId: 0,
                tabId: 1,
                timeStamp: 0,
                url: urlTarget
              }
            },
            {
              label: 'b-onBeforeNavigate',
              event: 'onBeforeNavigate',
              details: {
                documentLifecycle: 'active',
                frameId: 0,
                frameType: 'outermost_frame',
                parentFrameId: -1,
                processId: -1,
                tabId: 1,
                timeStamp: 0,
                url: urlTarget
              }
            },
            {
              label: 'b-onCommitted',
              event: 'onCommitted',
              details: {
                documentId: 2,
                documentLifecycle: 'active',
                frameId: 0,
                frameType: 'outermost_frame',
                parentFrameId: -1,
                processId: 0,
                tabId: 1,
                timeStamp: 0,
                transitionQualifiers: [],
                transitionType: 'link',
                url: urlTarget
              }
            },
            {
              label: 'b-onDOMContentLoaded',
              event: 'onDOMContentLoaded',
              details: {
                documentId: 2,
                documentLifecycle: 'active',
                frameId: 0,
                frameType: 'outermost_frame',
                parentFrameId: -1,
                processId: 0,
                tabId: 1,
                timeStamp: 0,
                url: urlTarget
              }
            },
            {
              label: 'b-onCompleted',
              event: 'onCompleted',
              details: {
                documentId: 2,
                documentLifecycle: 'active',
                frameId: 0,
                frameType: 'outermost_frame',
                parentFrameId: -1,
                processId: 0,
                tabId: 1,
                timeStamp: 0,
                url: urlTarget
              }
            }
          ],
          [
            navigationOrder('a-'), navigationOrder('b-'),
            [
              'a-onDOMContentLoaded', 'b-onCreatedNavigationTarget',
              'b-onBeforeNavigate'
            ]
          ]);

      // Notify the api test that we're waiting for the user.
      chrome.test.notifyPass();
    },
  ]);
});
