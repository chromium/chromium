// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onload = async function() {
  const getURL = chrome.runtime.getURL;
  const tab = await promise(chrome.tabs.create, {url: 'about:blank'});
  const config = await promise(chrome.test.getConfig);
  const port = config.testServer.port;

  const urlLoad = `http://127.0.0.1:${port}/prerender/prerender_loader.html`;
  const urlTarget = `http://127.0.0.1:${port}/prerender/prerender_page.html`;

  chrome.test.runTests([
    // A prerendered tab replaces the current tab.
    function prerendered() {
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
                url: urlLoad,
              },
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
                transitionType: 'typed',
                url: urlLoad,
              },
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
                url: urlLoad,
              },
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
                url: urlLoad,
              },
            },
            {
              label: 'b-onBeforeNavigate',
              event: 'onBeforeNavigate',
              details: {
                frameId: 0,
                parentFrameId: -1,
                processId: -1,
                tabId: 1,
                timeStamp: 0,
                url: urlTarget,
              },
            },
            {
              label: 'b-onCommitted',
              event: 'onCommitted',
              details: {
                frameId: 0,
                parentFrameId: -1,
                processId: 1,
                tabId: 1,
                timeStamp: 0,
                transitionQualifiers: [],
                transitionType: 'link',
                url: urlTarget,
              },
            },
            {
              label: 'b-onDOMContentLoaded',
              event: 'onDOMContentLoaded',
              details: {
                frameId: 0,
                parentFrameId: -1,
                processId: 1,
                tabId: 1,
                timeStamp: 0,
                url: urlTarget,
              },
            },
            {
              label: 'b-onCompleted',
              event: 'onCompleted',
              details: {
                frameId: 0,
                parentFrameId: -1,
                processId: 1,
                tabId: 1,
                timeStamp: 0,
                url: urlTarget,
              },
            },
            {
              label: 'onTabReplaced',
              event: 'onTabReplaced',
              details: {replacedTabId: 0, tabId: 1, timeStamp: 0},
            },
          ],
          [
            navigationOrder('a-'),
            navigationOrder('b-'),
            ['a-onCompleted', 'b-onCompleted', 'onTabReplaced'],
          ]);

      // Notify the api test that we're waiting for the user.
      chrome.test.notifyPass();
    },
  ]);
};
