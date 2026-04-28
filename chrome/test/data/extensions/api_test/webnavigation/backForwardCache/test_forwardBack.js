// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// There is no DOMContentLoaded event on restoring from the cache.
function cacheRestoreNavigationOrder(prefix) {
  return [
    `${prefix}onBeforeNavigate`,
    `${prefix}onCommitted`,
    `${prefix}onCompleted`,
  ];
}

onload = async function() {
  const tab = await promise(chrome.tabs.create, {url: 'about:blank'});
  const config = await promise(chrome.test.getConfig);
  const port = config.testServer.port;

  const urlA = `http://a.com:${port}/` +
      'extensions/api_test/webnavigation/backForwardCache/a.html';
  const urlB = `http://b.com:${port}/` +
      'extensions/api_test/webnavigation/backForwardCache/b.html';

  chrome.test.runTests([
    // First navigates to a.html which redirects to to b.html which uses
    // history.back() to navigate back to a.html
    function forwardBack() {
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
                url: urlA,
              },
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
                url: urlA,
              },
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
                url: urlA,
              },
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
                url: urlA,
              },
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
                tabId: 0,
                timeStamp: 0,
                url: urlB,
              },
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
                processId: 1,
                tabId: 0,
                timeStamp: 0,
                transitionQualifiers: [],
                transitionType: 'link',
                url: urlB,
              },
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
                processId: 1,
                tabId: 0,
                timeStamp: 0,
                url: urlB,
              },
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
                processId: 1,
                tabId: 0,
                timeStamp: 0,
                url: urlB,
              },
            },
            {
              label: 'c-onBeforeNavigate',
              event: 'onBeforeNavigate',
              details: {
                documentLifecycle: 'active',
                frameId: 0,
                frameType: 'outermost_frame',
                parentFrameId: -1,
                processId: -1,
                tabId: 0,
                timeStamp: 0,
                url: urlA,
              },
            },
            {
              label: 'c-onCommitted',
              event: 'onCommitted',
              details: {
                // documentId is the same as the first navigation
                // because the document was restored from the cache.
                documentId: 1,
                documentLifecycle: 'active',
                frameId: 0,
                frameType: 'outermost_frame',
                parentFrameId: -1,
                processId: 0,
                tabId: 0,
                timeStamp: 0,
                transitionQualifiers: ['forward_back'],
                transitionType: 'link',
                url: urlA,
              },
            },
            {
              label: 'c-onCompleted',
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
                url: urlA,
              },
            },
          ],
          [
            navigationOrder('a-'),
            navigationOrder('b-'),
            cacheRestoreNavigationOrder('c-'),
            isLoadedBy('b-', 'a-'),
            isLoadedBy('c-', 'b-'),
          ]);
      chrome.tabs.update(tab.id, {url: urlA});
    },
  ]);
};
