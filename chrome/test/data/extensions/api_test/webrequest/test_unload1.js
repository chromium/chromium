// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

runTests([
  // Opens a cross-origin iframe (a http-URL in this chrome-extension tab) and
  // removes it.
  function insertSlowCrossOriginFrameAndRemove() {
    const url = getSlowURL('frame-in-extension-url');
    const initiator = getServerDomain(initiators.BROWSER_INITIATED);

    expect([
      { label: 'onBeforeRequest',
        event: 'onBeforeRequest',
        details: {
          type: 'sub_frame',
          url,
          frameId: 1,
          parentFrameId: 0,
          frameUrl: url,
          tabId: 1,
          initiator: getInitiatorURLForExtension(),
          parentDocumentId: 1,
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
          tabId: 1,
          initiator: getInitiatorURLForExtension(),
          parentDocumentId: 1,
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
          tabId: 1,
          initiator: getInitiatorURLForExtension(),
          parentDocumentId: 1,
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
          tabId: 1,
          fromCache: false,
          error: 'net::ERR_ABORTED',
          initiator: getInitiatorURLForExtension(),
          parentDocumentId: 1,
          frameType: 'sub_frame',
        },
      }],
      [['onBeforeRequest', 'onBeforeSendHeaders', 'onSendHeaders',
        'onErrorOccurred']]);

    var f = document.createElement('iframe');
    f.src = url;
    waitUntilSendHeaders('sub_frame', url, function() {
      f.remove();
    });
    document.body.appendChild(f);
  },
]);
