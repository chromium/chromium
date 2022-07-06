// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var LOCAL_FOLDER = 'extensions/api_test/webrequest_public_session/';

function getResURL(url) {
  return getServerURL(LOCAL_FOLDER + url);
}

function getStrippedURL(url) {
  // In Public Session URL is stripped down to origin, that's why '' is used
  // here, otherwise the same as in getResURL would have to be used.
  return getServerURL('');
}

const scriptUrl =
      '_test_resources/api_test/webrequest_public_session/framework.js';
let loadScript = chrome.test.loadScript(scriptUrl);

loadScript.then(async function() {
  runTests([
  // Navigates to a page with a blocking handler that cancels the page request.
  // The page will not load. This tests the following:
  // - listeners can be registered on the webRequest API with empty URL
  //   permissions in manifest
  // - url is stripped down to origin
  // - cancel can be used (it's not blocked)
  // - extension can access arbitrary URL (all URLs are whitelisted for purposes
  //   of webRequest in Public Session); in case the extension cannot access the
  //   URL, this test will timeout.
  function cancelIsNotBlocked() {
    var targetUrl = getResURL('res/a.html');
    var expectUrl = getStrippedURL('res/a.html');
    expect(
      [  // events
        { label: 'onBeforeRequest',
          event: 'onBeforeRequest',
          details: {
            type: 'main_frame',
            url: expectUrl,
            frameUrl: expectUrl
          },
          retval: {cancel: true}
        },
        // Cancelling is considered an error.
        { label: 'onErrorOccurred',
          event: 'onErrorOccurred',
          details: {
            url: expectUrl,
            fromCache: false,
            error: 'net::ERR_BLOCKED_BY_CLIENT'
          }
        },
      ],
      [['onBeforeRequest', 'onErrorOccurred']], // event order
      {urls: ['<all_urls>']},  // filter
      ['blocking']);
    navigateAndWait(targetUrl);
  },

  // Tests that a redirect is successfully blocked (without the code that blocks
  // the redirect the following would error out with an unexpected event
  // 'onBeforeRedirect'). The test is good even with the URL stripped down to
  // origin because it doesn't rely on the URL for the check.
  function redirectIsBlocked() {
    var targetUrl = getURL('res/a.html');
    var redirectUrl = getURL('res/b.html');
    var expectUrl = getURL('');
    expect(
      [
        { label: 'onBeforeRequest',
          event: 'onBeforeRequest',
          details: {
            type: 'main_frame',
            url: expectUrl,
            frameUrl: expectUrl
          },
          retval: {redirectUrl: redirectUrl}
        },
        { label: 'onResponseStarted',
          event: 'onResponseStarted',
          details: {
            frameId: 0,
            fromCache: false,
            method: 'GET',
            parentFrameId: -1,
            statusCode: 200,
            statusLine: 'HTTP/1.1 200 OK',
            tabId: 0,
            type: 'main_frame',
            url: expectUrl
          }
        },
        { label: 'onCompleted',
          event: 'onCompleted',
          details: {
            frameId: 0,
            fromCache: false,
            method: 'GET',
            parentFrameId: -1,
            statusCode: 200,
            statusLine: 'HTTP/1.1 200 OK',
            tabId: 0,
            type: 'main_frame',
            url: expectUrl
          }
        },
      ],
      [['onBeforeRequest', 'onResponseStarted', 'onCompleted']],
      {urls: ['<all_urls>']},
      ['blocking']);
      navigateAndWait(targetUrl);
  },
])});
