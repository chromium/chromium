// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var pass = chrome.test.callbackPass;

// Constants as functions, not to be called until after runTests.
function getURLHttpWithScript() {
  return getServerURL('extensions/api_test/webrequest/script/index.html');
}

runTests([
  // Navigates to a page, with a blocking handler that cancels the navigation.
  function simpleLoadCancelledOnReceiveHeaders() {
    expect(
        [
          // events
          {
            label: 'onBeforeRequest',
            event: 'onBeforeRequest',
            details: {
              method: 'GET',
              type: 'main_frame',
              url: getURLHttpWithScript(),
              frameUrl: getURLHttpWithScript(),
              initiator: getServerDomain(initiators.BROWSER_INITIATED)
            },
            retval: {cancel: true},
          },
          {
            label: 'onErrorOccurred',
            event: 'onErrorOccurred',
            details: {
              url: getURLHttpWithScript(),
              initiator: getServerDomain(initiators.BROWSER_INITIATED),
              error: 'net::ERR_BLOCKED_BY_CLIENT',
              fromCache: false
            },
          },
        ],
        [  // event order
          [
            'onBeforeRequest',
            'onErrorOccurred',
          ]
        ],
        {urls: ['<all_urls>']},  // filter
        ['blocking']);
    navigateAndWait(getURLHttpWithScript());
  },
]);
