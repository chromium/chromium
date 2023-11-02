// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

runTests([
  // Open a page that loads a slow same-origin frame in it and remove the frame.
  function loadTabWithSlowFrameAndRemoveFrame() {
    const hostname = 'slow-child-frame-same-origin-remove-frame';
    const url = getSlowURL(hostname);
    const mainUrl = getPageWithFrame(url, hostname);

    expect([
      { label: 'onBeforeRequest',
        event: 'onBeforeRequest',
        details: {
          type: 'sub_frame',
          url,
          frameId: 1,
          parentFrameId: 0,
          frameUrl: url,
          parentDocumentId: 1,
          initiator: getInitiatorURLForHostname(hostname),
          frameType: 'sub_frame',
        }
      },
      { label: 'onBeforeSendHeaders',
        event: 'onBeforeSendHeaders',
        details: {
          type: 'sub_frame',
          url,
          frameId: 1,
          parentFrameId: 0,
          parentDocumentId: 1,
          initiator: getInitiatorURLForHostname(hostname),
          frameType: 'sub_frame',
        },
      },
      { label: 'onSendHeaders',
        event: 'onSendHeaders',
        details: {
          type: 'sub_frame',
          url,
          frameId: 1,
          parentFrameId: 0,
          parentDocumentId: 1,
          initiator: getInitiatorURLForHostname(hostname),
          frameType: 'sub_frame',
        },
      },
      { label: 'onErrorOccurred',
        event: 'onErrorOccurred',
        details: {
          type: 'sub_frame',
          url,
          frameId: 1,
          parentFrameId: 0,
          parentDocumentId: 1,
          initiator: getInitiatorURLForHostname(hostname),
          frameType: 'sub_frame',
          fromCache: false,
          error: 'net::ERR_ABORTED',
        },
      }],
      [['onBeforeRequest', 'onBeforeSendHeaders', 'onSendHeaders',
        'onErrorOccurred']],
      {
        urls: ['<all_urls>'],
        types: ['sub_frame'],
      });

    waitUntilSendHeaders('sub_frame', url, function() {
      // Cancels load and triggers onErrorOccurred.
      chrome.tabs.executeScript(tabId, {
        code: 'document.querySelector("iframe").remove();',
      });
    });
    chrome.tabs.update(tabId, {url: mainUrl});
  },

  // Now reload the page (so that the frame appears again) and remove the tab.
  // The expectations are identical to the previous test.
  function openTabWithSlowFrameAndRemoveTab() {
    const hostname = 'slow-child-frame-same-origin-remove-tab';
    const url = getSlowURL(hostname);
    const mainUrl = getPageWithFrame(url, hostname);

    expect([
      { label: 'onBeforeRequest',
        event: 'onBeforeRequest',
        details: {
          type: 'sub_frame',
          url,
          frameId: 1,
          parentFrameId: 0,
          frameUrl: url,
          parentDocumentId: 1,
          initiator: getInitiatorURLForHostname(hostname),
          frameType: 'sub_frame',
        }
      },
      { label: 'onBeforeSendHeaders',
        event: 'onBeforeSendHeaders',
        details: {
          type: 'sub_frame',
          url,
          frameId: 1,
          parentFrameId: 0,
          parentDocumentId: 1,
          initiator: getInitiatorURLForHostname(hostname),
          frameType: 'sub_frame',
        },
      },
      { label: 'onSendHeaders',
        event: 'onSendHeaders',
        details: {
          type: 'sub_frame',
          url,
          frameId: 1,
          parentFrameId: 0,
          parentDocumentId: 1,
          initiator: getInitiatorURLForHostname(hostname),
          frameType: 'sub_frame',
        },
      },
      { label: 'onErrorOccurred',
        event: 'onErrorOccurred',
        details: {
          type: 'sub_frame',
          url,
          frameId: 1,
          parentFrameId: 0,
          fromCache: false,
          error: 'net::ERR_ABORTED',
          parentDocumentId: 1,
          initiator: getInitiatorURLForHostname(hostname),
          frameType: 'sub_frame',
        },
      }],
      [['onBeforeRequest', 'onBeforeSendHeaders', 'onSendHeaders',
        'onErrorOccurred']],
      {
        urls: ['<all_urls>'],
        types: ['sub_frame'],
      });

    var callbackDone = chrome.test.callbackAdded();

    waitUntilSendHeaders('sub_frame', url, function() {
      // Cancels load and triggers onErrorOccurred.
      chrome.tabs.remove(tabId, callbackDone);
    });
    chrome.tabs.update(tabId, {url: mainUrl});
  },
]);
