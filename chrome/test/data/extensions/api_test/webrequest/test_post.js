// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Common definitions for test_post?.js.

var dirName = "requestBody/";

function sendPost(formFile, parseableForm) {
  // The following variables must be updated when files in |dirName| change.
  var formData = {
      "check": ["option_A"],
      "password": ["password"],
      "radio": ["Yes"],
      "select": ["one"],
      "text\"1\u011B\u0161\u00FD\u4EBA\r\n \r\n": ["TEST_TEXT_1"],
      "text2": ["TEST_TEXT_2"],
      "text3": ["TEST_TEXT_3"],
      "txtarea": ["text\"1\u011B\u0161\u00FD\u4EBA\r\n \r\n"]
  };
  return function submitForm() {
    expect(
      [  // events
        { label: "a-onBeforeRequest",
          event: "onBeforeRequest",
          details: {
            method: "GET",
            type: "main_frame",
            url: getURL(dirName + formFile),
            frameUrl: getURL(dirName + formFile),
            initiator: getDomain(initiators.BROWSER_INITIATED)
          }
        },
        { label: "a-onResponseStarted",
          event: "onResponseStarted",
          details: {
            fromCache: false,
            method: "GET",
            statusCode: 200,
            statusLine: "HTTP/1.1 200 OK",
            type: "main_frame",
            url: getURL(dirName + formFile),
            initiator: getDomain(initiators.BROWSER_INITIATED)
          }
        },
        { label: "a-onCompleted",
          event: "onCompleted",
          details: {
            fromCache: false,
            method: "GET",
            statusCode: 200,
            statusLine: "HTTP/1.1 200 OK",
            type: "main_frame",
            url: getURL(dirName + formFile),
            initiator: getDomain(initiators.BROWSER_INITIATED)
          }
        },
        { label: "s-onBeforeRequest",
          event: "onBeforeRequest",
          details: {
            method: "GET",
            type: "script",
            url: getURL("requestBody/submit.js"),
            frameUrl: getURL(dirName + formFile),
            initiator: getDomain(initiators.WEB_INITIATED)
          }
        },
        { label: "s-onResponseStarted",
          event: "onResponseStarted",
          details: {
            fromCache: false,
            method: "GET",
            statusCode: 200,
            statusLine: "HTTP/1.1 200 OK",
            type: "script",
            url: getURL("requestBody/submit.js"),
            initiator: getDomain(initiators.WEB_INITIATED)
          }
        },
        { label: "s-onCompleted",
          event: "onCompleted",
          details: {
            fromCache: false,
            method: "GET",
            statusCode: 200,
            statusLine: "HTTP/1.1 200 OK",
            type: "script",
            url: getURL("requestBody/submit.js"),
            initiator: getDomain(initiators.WEB_INITIATED)
          }
        },
        { label: "b-onBeforeRequest",
          event: "onBeforeRequest",
          details: {
            method: "POST",
            type: "main_frame",
            url: getURL("simpleLoad/a.html"),
            frameUrl: getURL("simpleLoad/a.html"),
            requestBody: parseableForm ? {
              formData: formData
            } : {
              raw: [{bytes: new ArrayBuffer}] // wildcard: matches any buffer
            },
            initiator: getDomain(initiators.WEB_INITIATED)
          }
        },
        { label: "b-onResponseStarted",
          event: "onResponseStarted",
          details: {
            fromCache: false,
            method: "POST",
            statusCode: 200,
            statusLine: "HTTP/1.1 200 OK",
            type: "main_frame",
            url: getURL("simpleLoad/a.html"),
            initiator: getDomain(initiators.WEB_INITIATED)
          }
        },
        { label: "b-onCompleted",
          event: "onCompleted",
          details: {
            fromCache: false,
            method: "POST",
            statusCode: 200,
            statusLine: "HTTP/1.1 200 OK",
            type: "main_frame",
            url: getURL("simpleLoad/a.html"),
            initiator: getDomain(initiators.WEB_INITIATED)
          }
        }
      ],
      [  // event order
        ["a-onBeforeRequest", "a-onResponseStarted", "a-onCompleted",
         "s-onBeforeRequest", "s-onResponseStarted", "s-onCompleted",
         "b-onBeforeRequest", "b-onResponseStarted", "b-onCompleted"]
      ],
      {urls: ["<all_urls>"]},  // filter
      ["requestBody"]);
    navigateAndWait(getURL(dirName + formFile));
  }
}
