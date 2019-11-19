// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

runTests([
  // Starts an XMLHttpRequest in an iframe and removes the frame.
  // Unlike the previous tests (test_unload1, 2, 3, 4), this test doesn't wait
  // for a server response and immediately removes the frame after issuing an
  // asynchronous request.
  function startXMLHttpRequestAndRemoveFrame() {
    const hostname = 'slow-resourcetype-xhr-immediately-remove-frame';
    const url = getSlowURL(hostname);
    const initiator = getServerDomain(initiators.WEB_INITIATED, hostname)
    const mainUrl = getPageWithFrame('empty.html', hostname);

    expect([
      { label: 'onBeforeRequest',
        event: 'onBeforeRequest',
        details: {
          type: 'xmlhttprequest',
          url,
          frameId: 1,
          parentFrameId: 0,
          frameUrl: 'unknown frame URL',
          initiator: initiator
        }
      },
      { label: 'onBeforeSendHeaders',
        event: 'onBeforeSendHeaders',
        details: {
          type: 'xmlhttprequest',
          url,
          frameId: 1,
          parentFrameId: 0,
          initiator: initiator
        },
      },
      { label: 'onSendHeaders',
        event: 'onSendHeaders',
        details: {
          type: 'xmlhttprequest',
          url,
          frameId: 1,
          parentFrameId: 0,
          initiator: initiator
        },
      },
      { label: 'onErrorOccurred',
        event: 'onErrorOccurred',
        details: {
          type: 'xmlhttprequest',
          url,
          frameId: 1,
          parentFrameId: 0,
          fromCache: false,
          error: 'net::ERR_ABORTED',
          initiator: initiator
        },
      }],
      [['onBeforeRequest', 'onBeforeSendHeaders', 'onSendHeaders',
        'onErrorOccurred']],
      {
        urls: ['<all_urls>'],
        types: ['xmlhttprequest'],
      });

    navigateAndWait(mainUrl, function() {
      chrome.tabs.executeScript(tabId, {
        allFrames: true,
        code: `if (top !== window) {
          var x = new XMLHttpRequest();
          x.open('GET', '${url}', true);
          x.send();
          frameElement.remove();
        }`
      });
    });
  },

  // Starts an XMLHttpRequest in the main frame and immediately remove the tab.
  function startXMLHttpRequestAndRemoveTab() {
    const hostname = 'slow-resourcetype-xhr-immediately-remove-tab';
    const url = getSlowURL(hostname);
    const initiator = getServerDomain(initiators.WEB_INITIATED, hostname)
    const mainUrl = getServerURL('empty.html', hostname);

    expect([
      { label: 'onBeforeRequest',
        event: 'onBeforeRequest',
        details: {
          type: 'xmlhttprequest',
          url,
          frameUrl: 'unknown frame URL',
          tabId: 1,
          initiator: initiator
        }
      },
      { label: 'onBeforeSendHeaders',
        event: 'onBeforeSendHeaders',
        details: {
          type: 'xmlhttprequest',
          url,
          tabId: 1,
          initiator: initiator
        },
      },
      { label: 'onSendHeaders',
        event: 'onSendHeaders',
        details: {
          type: 'xmlhttprequest',
          url,
          tabId: 1,
          initiator: initiator
        },
      },
      { label: 'onErrorOccurred',
        event: 'onErrorOccurred',
        details: {
          type: 'xmlhttprequest',
          url,
          fromCache: false,
          error: 'net::ERR_ABORTED',
          tabId: 1,
          initiator: initiator
        },
      }],
      [['onBeforeRequest', 'onBeforeSendHeaders', 'onSendHeaders',
        'onErrorOccurred']],
      {
        urls: ['<all_urls>'],
        types: ['xmlhttprequest'],
      });

    var callbackDone = chrome.test.callbackAdded();

    // Creating a new tab instead of re-using the tab from the test framework
    // because a page can only close itself if it has no navigation history.
    chrome.tabs.create({
      url: mainUrl,
    }, function(tab) {
      chrome.tabs.executeScript(tab.id, {
        code: `
          var x = new XMLHttpRequest();
          x.open('GET', '${url}', true);
          x.send();
          window.close();
          // Closing the tab should not be blocked. If it does, then the slow
          // request will eventually complete and cause a test failure.
        `
      }, callbackDone);
    });
  },
]);
