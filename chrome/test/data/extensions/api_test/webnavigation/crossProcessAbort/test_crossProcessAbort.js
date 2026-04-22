// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onload = async function() {
  const tab = await promise(chrome.tabs.create, {url: 'about:blank'});
  const config = await promise(chrome.test.getConfig);
  const port = config.testServer.port;
  const getURL = chrome.runtime.getURL;

  const initialUrl = getURL('initial.html');
  const sameSiteUrl = getURL('empty.html');
  const crossSiteUrl = `http://127.0.0.1:${port}/title1.html`;

  chrome.test.runTests([
    // Navigates to a slow cross-site URL (extension to HTTP) and starts
    // a slow renderer-initiated, non-user, same-site navigation.
    // The cross-site navigation commits while the same-site navigation
    // is in process and this test expects an error event for the
    // same-site navigation.
    function crossProcessAbort() {
      expect(
          [
            {
              label: 'a-onBeforeNavigate',
              event: 'onBeforeNavigate',
              details: {
                frameId: 0,
                parentFrameId: -1,
                processId: -1,
                tabId: 0,
                timeStamp: 0,
                url: initialUrl
              }
            },
            {
              label: 'a-onCommitted',
              event: 'onCommitted',
              details: {
                frameId: 0,
                parentFrameId: -1,
                processId: 0,
                tabId: 0,
                timeStamp: 0,
                transitionQualifiers: [],
                transitionType: 'link',
                url: initialUrl
              }
            },
            {
              label: 'a-onDOMContentLoaded',
              event: 'onDOMContentLoaded',
              details: {
                frameId: 0,
                parentFrameId: -1,
                processId: 0,
                tabId: 0,
                timeStamp: 0,
                url: initialUrl
              }
            },
            {
              label: 'a-onCompleted',
              event: 'onCompleted',
              details: {
                frameId: 0,
                parentFrameId: -1,
                processId: 0,
                tabId: 0,
                timeStamp: 0,
                url: initialUrl
              }
            },
            {
              label: 'b-onBeforeNavigate',
              event: 'onBeforeNavigate',
              details: {
                frameId: 0,
                parentFrameId: -1,
                processId: -1,
                tabId: 0,
                timeStamp: 0,
                url: crossSiteUrl
              }
            },
            {
              label: 'b-onCommitted',
              event: 'onCommitted',
              details: {
                frameId: 0,
                parentFrameId: -1,
                processId: 1,
                tabId: 0,
                timeStamp: 0,
                transitionQualifiers: [],
                transitionType: 'link',
                url: crossSiteUrl
              }
            },
            {
              label: 'b-onDOMContentLoaded',
              event: 'onDOMContentLoaded',
              details: {
                frameId: 0,
                parentFrameId: -1,
                processId: 1,
                tabId: 0,
                timeStamp: 0,
                url: crossSiteUrl
              }
            },
            {
              label: 'b-onCompleted',
              event: 'onCompleted',
              details: {
                frameId: 0,
                parentFrameId: -1,
                processId: 1,
                tabId: 0,
                timeStamp: 0,
                url: crossSiteUrl
              }
            },
            {
              label: 'c-onBeforeNavigate',
              event: 'onBeforeNavigate',
              details: {
                frameId: 0,
                parentFrameId: -1,
                processId: -1,
                tabId: 0,
                timeStamp: 0,
                url: sameSiteUrl
              }
            },
            {
              label: 'c-onErrorOccurred',
              event: 'onErrorOccurred',
              details: {
                error: 'net::ERR_ABORTED',
                frameId: 0,
                parentFrameId: -1,
                processId: -1,
                tabId: 0,
                timeStamp: 0,
                url: sameSiteUrl
              }
            },
          ],
          [
            navigationOrder('a-'), navigationOrder('b-'),
            [
              'a-onCompleted', 'b-onBeforeNavigate', 'c-onBeforeNavigate',
              'c-onErrorOccurred', 'b-onCommitted'
            ]
          ]);

      chrome.tabs.update(
          tab.id,
          {url: getURL(`initial.html?${port}/title1.html`)},
      );
    },
  ]);
};
