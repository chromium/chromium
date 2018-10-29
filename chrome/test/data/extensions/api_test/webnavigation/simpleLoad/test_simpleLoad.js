// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onload = async function() {
  let getURL = chrome.extension.getURL;
  let tab = await promise(chrome.tabs.create, {"url": "about:blank"});

  chrome.test.runTests([
    // Navigates to an URL.
    function simpleLoad() {
      expect([
        { label: "a-onBeforeNavigate",
          event: "onBeforeNavigate",
          details: { frameId: 0,
                     parentFrameId: -1,
                     processId: -1,
                     tabId: 0,
                     timeStamp: 0,
                     url: getURL('a.html') }},
        { label: "a-onCommitted",
          event: "onCommitted",
          details: { frameId: 0,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     transitionQualifiers: [],
                     transitionType: "link",
                     url: getURL('a.html') }},
        { label: "a-onDOMContentLoaded",
          event: "onDOMContentLoaded",
          details: { frameId: 0,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     url: getURL('a.html') }},
        { label: "a-onCompleted",
          event: "onCompleted",
          details: { frameId: 0,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     url: getURL('a.html') }}],
        [ navigationOrder("a-") ]);
      chrome.tabs.update(tab.id, { url: getURL('a.html') });
    },
  ]);
};
