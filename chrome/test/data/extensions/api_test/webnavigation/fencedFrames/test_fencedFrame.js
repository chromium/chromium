// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const scriptUrl = '_test_resources/api_test/webnavigation/framework.js';
let loadScript = chrome.test.loadScript(scriptUrl);

loadScript.then(async function() {
  let getURL = chrome.runtime.getURL;
  let tab = await promise(chrome.tabs.create, {url: 'about:blank'});
  let config = await promise(chrome.test.getConfig);
  let port = config.testServer.port;
  let URL_MAIN = getURL('main.html');
  let URL_INTERMEDIATE_IFRAME = getURL('iframe.html');
  let URL_FENCED_FRAME = 'http://a.com:' + port +
      '/extensions/api_test/webnavigation/fencedFrames/frame.html';
  chrome.test.runTests([
    // Navigates from an extension page to a HTTP page to contain
    // an iframe which contains a fenced frame.
    // Tests that the frameId/parentFrameId are populated correctly.
    function fencedFrameNavigation() {
      expect([
        { label: 'main-onBeforeNavigate',
          event: 'onBeforeNavigate',
          details: { frameId: 0,
                     parentFrameId: -1,
                     processId: -1,
                     tabId: 0,
                     timeStamp: 0,
                     url: URL_MAIN }},
        { label: 'main-onCommitted',
          event: 'onCommitted',
          details: { frameId: 0,
                     parentFrameId: -1,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     transitionQualifiers: [],
                     transitionType: 'link',
                     url: URL_MAIN }},
        { label: 'main-onDOMContentLoaded',
          event: 'onDOMContentLoaded',
          details: { frameId: 0,
                     parentFrameId: -1,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     url: URL_MAIN }},
        { label: 'main-onCompleted',
          event: 'onCompleted',
          details: { frameId: 0,
                     parentFrameId: -1,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     url: URL_MAIN }},
        { label: 'intermediate-onBeforeNavigate',
          event: 'onBeforeNavigate',
          details: { frameId: 1,
                     parentFrameId: 0,
                     processId: -1,
                     tabId: 0,
                     timeStamp: 0,
                     url: URL_INTERMEDIATE_IFRAME }},
        { label: 'intermediate-onCommitted',
          event: 'onCommitted',
          details: { frameId: 1,
                     parentFrameId: 0,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     transitionQualifiers: [],
                     transitionType: 'auto_subframe',
                     url: URL_INTERMEDIATE_IFRAME }},
        { label: 'intermediate-onDOMContentLoaded',
          event: 'onDOMContentLoaded',
          details: { frameId: 1,
                     parentFrameId: 0,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     url: URL_INTERMEDIATE_IFRAME }},
        { label: 'intermediate-onCompleted',
          event: 'onCompleted',
          details: { frameId: 1,
                     parentFrameId: 0,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     url: URL_INTERMEDIATE_IFRAME }},
        { label: 'a.com-onBeforeNavigate',
          event: 'onBeforeNavigate',
          details: { frameId: 2,
                     parentFrameId: 1,
                     processId: -1,
                     tabId: 0,
                     timeStamp: 0,
                     url: URL_FENCED_FRAME }},
        { label: 'a.com-onCommitted',
          event: 'onCommitted',
          details: { frameId: 2,
                     parentFrameId: 1,
                     processId: 1,
                     tabId: 0,
                     timeStamp: 0,
                     transitionQualifiers: [],
                     transitionType: 'auto_subframe',
                     url: URL_FENCED_FRAME }},
        { label: 'a.com-onDOMContentLoaded',
          event: 'onDOMContentLoaded',
          details: { frameId: 2,
                     parentFrameId: 1,
                     processId: 1,
                     tabId: 0,
                     timeStamp: 0,
                     url: URL_FENCED_FRAME }},
        { label: 'a.com-onCompleted',
          event: 'onCompleted',
          details: { frameId: 2,
                     parentFrameId: 1,
                     processId: 1,
                     tabId: 0,
                     timeStamp: 0,
                     url: URL_FENCED_FRAME }}],
        [
          navigationOrder('main-'),
          navigationOrder('intermediate-'),
          navigationOrder('a.com-'),
        ]);

      chrome.tabs.update(tab.id, {url: URL_MAIN});
    },

    function testGetAllFrames() {
      chrome.webNavigation.getAllFrames({tabId: tab.id}, function (details) {
          // Since processIds are randomly assigned we remove them for the
          // assertEq.
          details.forEach(element => delete element.processId);
          chrome.test.assertEq(
              [{errorOccurred: false,
                frameId: 0,
                parentFrameId: -1,
                url: URL_MAIN},
              {errorOccurred: false,
                frameId: 4,
                parentFrameId: 0,
                url: URL_INTERMEDIATE_IFRAME},
              {errorOccurred: false,
                frameId: 5,
                parentFrameId: 4,
                url: URL_FENCED_FRAME}],
               details);
          chrome.test.succeed();
      });
    },
  ]);
});
