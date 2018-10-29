// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onload = async function() {
  let getURL = chrome.extension.getURL;
  let tab = await promise(chrome.tabs.create, {"url": "about:blank"});

  chrome.test.runTests([
    // Opens a tab and waits for the user to middle-click on a link in it.
    function requestOpenTab() {
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
                     transitionType: "typed",
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
                     url: getURL('a.html') }},
        { label: "b-onCreatedNavigationTarget",
          event: "onCreatedNavigationTarget",
          details: { sourceFrameId: 0,
                     sourceProcessId: 0,
                     sourceTabId: 0,
                     tabId: 1,
                     timeStamp: 0,
                     url: getURL('b.html') }},
        { label: "b-onBeforeNavigate",
          event: "onBeforeNavigate",
          details: { frameId: 0,
                     parentFrameId: -1,
                     processId: -1,
                     tabId: 1,
                     timeStamp: 0,
                     url: getURL('b.html') }},
        { label: "b-onCommitted",
          event: "onCommitted",
          details: { frameId: 0,
                     processId: 0,
                     tabId: 1,
                     timeStamp: 0,
                     transitionQualifiers: [],
                     transitionType: "link",
                     url: getURL('b.html') }},
        { label: "b-onDOMContentLoaded",
          event: "onDOMContentLoaded",
          details: { frameId: 0,
                     processId: 0,
                     tabId: 1,
                     timeStamp: 0,
                     url: getURL('b.html') }},
        { label: "b-onCompleted",
          event: "onCompleted",
          details: { frameId: 0,
                     processId: 0,
                     tabId: 1,
                     timeStamp: 0,
                     url: getURL('b.html') }}],
        [ navigationOrder("a-"),
          navigationOrder("b-"),
          [ "a-onDOMContentLoaded",
            "b-onCreatedNavigationTarget",
            "b-onBeforeNavigate" ]]);

      // Notify the api test that we're waiting for the user.
      chrome.test.notifyPass();
    },
  ]);
};
