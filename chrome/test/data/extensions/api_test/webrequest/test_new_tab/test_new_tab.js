// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const scriptUrl = '_test_resources/api_test/webrequest/framework.js';
let loadScript = chrome.test.loadScript(scriptUrl);

loadScript.then(async function() {
  runTests([
  // Navigates to a page with a link with target=_blank. Then simulates a click
  // on that link and verifies that the new tab has a correct tab ID assigned.
  function setup () {
    expect(
      [  // events
        { label: "a-onBeforeRequest",
          event: "onBeforeRequest",
          details: {
            url: getURL("newTab/a.html"),
            frameUrl: getURL("newTab/a.html"),
            initiator: getDomain(initiators.BROWSER_INITIATED)
          }
        },
        { label: "a-onResponseStarted",
          event: "onResponseStarted",
          details: {
            url: getURL("newTab/a.html"),
            statusCode: 200,
            fromCache: false,
            statusLine: "HTTP/1.1 200 OK",
            initiator: getDomain(initiators.BROWSER_INITIATED)
            // Request to chrome-extension:// url has no IP.
          }
        },
        { label: "a-onCompleted",
          event: "onCompleted",
          details: {
            url: getURL("newTab/a.html"),
            statusCode: 200,
            fromCache: false,
            statusLine: "HTTP/1.1 200 OK",
            initiator: getDomain(initiators.BROWSER_INITIATED)
           // Request to chrome-extension:// url has no IP.
          }
        },
        { label: "b-onBeforeRequest",
          event: "onBeforeRequest",
          details: {
            url: getURL("newTab/b.html"),
            frameUrl: getURL("newTab/b.html"),
            tabId: 1,
            initiator: getDomain(initiators.WEB_INITIATED)
          }
        },
        { label: "b-onResponseStarted",
          event: "onResponseStarted",
          details: {
            url: getURL("newTab/b.html"),
            statusCode: 200,
            fromCache: false,
            statusLine: "HTTP/1.1 200 OK",
            // Request to chrome-extension:// url has no IP.
            tabId: 1,
            initiator: getDomain(initiators.WEB_INITIATED)
          }
        },
        { label: "b-onCompleted",
          event: "onCompleted",
          details: {
            url: getURL("newTab/b.html"),
            statusCode: 200,
            fromCache: false,
            statusLine: "HTTP/1.1 200 OK",
            // Request to chrome-extension:// url has no IP.
            tabId: 1,
            initiator: getDomain(initiators.WEB_INITIATED)
          }
        },
      ],
      [  // event order
      ["a-onBeforeRequest", "a-onResponseStarted", "a-onCompleted",
       "b-onBeforeRequest", "b-onResponseStarted", "b-onCompleted"] ]);
    // Notify the api test that we're waiting for the user.
    chrome.test.notifyPass();
  },
])});
