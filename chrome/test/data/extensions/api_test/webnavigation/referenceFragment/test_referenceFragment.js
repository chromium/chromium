// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onload = async function() {
  var getURL = chrome.extension.getURL;
  let tab = await promise(chrome.tabs.create, {"url": "about:blank"});

  chrome.test.runTests([
    // Reference fragment navigation.
    function referenceFragment() {
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
                     url: getURL('a.html#anchor') }},
        { label: "a-onReferenceFragmentUpdated",
          event: "onReferenceFragmentUpdated",
          details: { frameId: 0,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     transitionQualifiers: ["client_redirect"],
                     transitionType: "link",
                     url: getURL('a.html#anchor') }}
      ],
      [ navigationOrder("a-") ]);

      chrome.tabs.update(tab.id, { url: getURL('a.html') });
    },

    // This is a test case in which a fragment navigation as a result of a
    // click is followed by a load of the original URL. The load of the
    // original URL has the potential to be confused for a fragment
    // navigation, since they differ only in the fragment.
    // See https://crbug.com/732083.
    function locationAssignAfterFragment() {
      expect([
        { label: "b-onBeforeNavigate",
          event: "onBeforeNavigate",
          details: { frameId: 0,
                     parentFrameId: -1,
                     processId: -1,
                     tabId: 0,
                     timeStamp: 0,
                     url: getURL('b.html') }},
        { label: "b-onCommitted",
          event: "onCommitted",
          details: { frameId: 0,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     transitionQualifiers: [],
                     transitionType: "link",
                     url: getURL('b.html') }},
        { label: "b-onDOMContentLoaded",
          event: "onDOMContentLoaded",
          details: { frameId: 0,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     url: getURL('b.html') }},
        { label: "b-onCompleted",
          event: "onCompleted",
          details: { frameId: 0,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     url: getURL('b.html') }},
        { label: "b-onReferenceFragmentUpdated",
          event: "onReferenceFragmentUpdated",
          details: { frameId: 0,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     transitionQualifiers: [],
                     transitionType: "link",
                     url: getURL('b.html#') }},
        { label: "b1-onBeforeNavigate",
          event: "onBeforeNavigate",
          details: { frameId: 0,
                     parentFrameId: -1,
                     processId: -1,
                     tabId: 0,
                     timeStamp: 0,
                     url: getURL('b.html') }},
        { label: "b1-onCommitted",
          event: "onCommitted",
          details: { frameId: 0,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     transitionQualifiers: ["client_redirect"],
                     transitionType: "link",
                     url: getURL('b.html') }},
        { label: "b1-onDOMContentLoaded",
          event: "onDOMContentLoaded",
          details: { frameId: 0,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     url: getURL('b.html') }},
        { label: "b1-onCompleted",
          event: "onCompleted",
          details: { frameId: 0,
                     processId: 0,
                     tabId: 0,
                     timeStamp: 0,
                     url: getURL('b.html') }},

      ],
      [ navigationOrder("b-"), navigationOrder("b1-") ]);

      chrome.tabs.update(tab.id, { url: getURL('b.html') });
    },
  ]);
};
