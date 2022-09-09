// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

runTests([
  // Loads a slow image in an iframe and removes it.
  function loadImageAndRemoveFrame() {
    const hostname = 'slow-resourcetype-image-remove-frame';
    const url = getSlowURL(hostname);
    const mainUrl = getPageWithFrame('empty.html', hostname);

    expect([
      { label: 'onBeforeRequest',
        event: 'onBeforeRequest',
        details: {
          type: 'image',
          url,
          frameId: 1,
          parentFrameId: 0,
          frameUrl: 'unknown frame URL',
          initiator: getInitiatorURLForHostname(hostname),
          parentDocumentId: 1,
          documentId: 2,
          documentLifecycle: "active",
          tabId: 0,
          frameType: "sub_frame",
         }
      },
      { label: 'onBeforeSendHeaders',
        event: 'onBeforeSendHeaders',
        details: {
          type: 'image',
          url,
          frameId: 1,
          parentFrameId: 0,
          initiator: getInitiatorURLForHostname(hostname),
          parentDocumentId: 1,
          documentId: 2,
          documentLifecycle: "active",
          tabId: 0,
          frameType: "sub_frame",
        },
      },
      { label: 'onSendHeaders',
        event: 'onSendHeaders',
        details: {
          type: 'image',
          url,
          frameId: 1,
          parentFrameId: 0,
          initiator: getInitiatorURLForHostname(hostname),
          parentDocumentId: 1,
          documentId: 2,
          documentLifecycle: "active",
          tabId: 0,
          frameType: "sub_frame",
        },
      },
      { label: 'onErrorOccurred',
        event: 'onErrorOccurred',
        details: {
          type: 'image',
          url,
          frameId: 1,
          parentFrameId: 0,
          initiator: getInitiatorURLForHostname(hostname),
          parentDocumentId: 1,
          documentId: 2,
          documentLifecycle: "active",
          tabId: 0,
          frameType: "sub_frame",
          fromCache: false,
          error: 'net::ERR_ABORTED',
        },
      }],
      [['onBeforeRequest', 'onBeforeSendHeaders', 'onSendHeaders',
        'onErrorOccurred']],
      {
        urls: ['<all_urls>'],
        types: ['image'],
      });

    waitUntilSendHeaders('image', url, function() {
      // Cancels load and triggers onErrorOccurred.
      chrome.tabs.executeScript(tabId, {
        code: 'document.querySelector("iframe").remove();',
      });
    });

    navigateAndWait(mainUrl, function() {
      chrome.tabs.executeScript(tabId, {
        allFrames: true,
        code: `if (top !== window) {
          var img = new Image();
          img.src = '${url}';
        }`
      });
    });
  },

  // Load a slow image in the main frame and close the tab.
  function loadImageAndRemoveTab() {
    const hostname = 'slow-resourcetype-image-remove-tab';
    const url = getSlowURL(hostname);
    const mainUrl = getServerURL('empty.html', hostname);

    expect([
      { label: 'onBeforeRequest',
        event: 'onBeforeRequest',
        details: {
          type: 'image',
          url: url,
          frameUrl: 'unknown frame URL',
          initiator: getInitiatorURLForHostname(hostname),
          documentId: 1,
          tabId: 0,
          frameId: 0,
          parentFrameId: -1,
          documentLifecycle: "active",
          frameType: "outermost_frame"
        }
      },
      { label: 'onBeforeSendHeaders',
        event: 'onBeforeSendHeaders',
        details: {
          type: 'image',
          url: url,
          initiator: getInitiatorURLForHostname(hostname),
          documentId: 1,
          tabId: 0,
          frameId: 0,
          parentFrameId: -1,
          documentLifecycle: "active",
          frameType: "outermost_frame"
        },
      },
      { label: 'onSendHeaders',
        event: 'onSendHeaders',
        details: {
          type: 'image',
          url: url,
          initiator: getInitiatorURLForHostname(hostname),
          documentId: 1,
          tabId: 0,
          frameId: 0,
          parentFrameId: -1,
          documentLifecycle: "active",
          frameType: "outermost_frame",
        },
      },
      { label: 'onErrorOccurred',
        event: 'onErrorOccurred',
        details: {
          type: 'image',
          url: url,
          initiator: getInitiatorURLForHostname(hostname),
          documentId: 1,
          tabId: 0,
          frameId: 0,
          parentFrameId: -1,
          documentLifecycle: "active",
          frameType: "outermost_frame",
          fromCache: false,
          error: 'net::ERR_ABORTED',
        },
      }],
      [['onBeforeRequest', 'onBeforeSendHeaders', 'onSendHeaders',
        'onErrorOccurred']],
      {
        urls: ['<all_urls>'],
        types: ['image'],
      });

    var callbackDone = chrome.test.callbackAdded();

    waitUntilSendHeaders('image', url, function() {
      // Cancels load and triggers onErrorOccurred.
      chrome.tabs.remove(tabId, callbackDone);
    }),

    navigateAndWait(mainUrl, function() {
      chrome.tabs.executeScript(tabId, {
        code: `var img = new Image(); img.src = '${url}';`
      });
    });
  },
]);
