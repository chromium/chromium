// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var expectedEventData;
var capturedEventData;
var shouldIgnore;

function expect(data, ignoreFunc) {
  expectedEventData = data;
  capturedEventData = [];
  shouldIgnore = ignoreFunc;
}

function checkExpectations() {
  if (capturedEventData.length < expectedEventData.length) {
    return;
  }
  chrome.test.assertEq(JSON.stringify(expectedEventData),
      JSON.stringify(capturedEventData));
  chrome.test.succeed();
}

var getURL = chrome.extension.getURL;

chrome.tabs.onUpdated.addListener(function(tabId, info, tab) {
  console.log('---onUpdated: ' + info.status + ', ' + info.url + '. ' +
               info.favIconUrl);
  if (shouldIgnore && shouldIgnore(info)) {
    return;
  }
  capturedEventData.push(info);
  checkExpectations();
});

chrome.test.runTests([
  function browserThenRendererInitiated() {
    // Note that a.html will set it's location.href to b.html, creating a
    // renderer-initiated navigation.
    expect([
      { status: 'loading', url: getURL('browserThenRendererInitiated/a.html') },
      { status: 'loading', url: getURL('browserThenRendererInitiated/b.html') },
      { status: 'complete' },
    ]);

    chrome.tabs.create({ url: getURL('browserThenRendererInitiated/a.html') });
  },

  function chromeUrls() {
    // Test for crbug.com/27208.
    expect([
      { status: 'loading', url: 'chrome://chrome-urls/' },
      { title : "Chrome URLs" },
      { status: 'complete' }
    ]);

    chrome.tabs.create({ url: 'chrome://chrome-urls/' });
  },

  /*
  // TODO(rafaelw) -- This is disabled because this test is flakey.
  function updateDuringCreateCallback() {
    // Test for crbug.com/27204.
    // We have to ignore anything that comes before the about:blank loading
    // status.
    var ignore = true;
    expect([
      { status: 'loading', url: 'about:blank' },
      { status: 'complete' }
    ], function(info) {
      if (info.status === 'loading' && info.url === 'about:blank') {
        ignore = false;
      }
      return ignore;
    });

    chrome.tabs.create({ url: 'chrome://newtab/' }, function(tab) {
      chrome.tabs.update(tab.id, { url: 'about:blank' });
    });
  }, */

  function iframeNavigated() {
    // The sequence of events goes like this:
    // -a.html starts loading
    // -while a.html is loading, iframe1.html (in it's onload) navigates to
    // iframe2.html. This causes the page to continue to be in the loading state
    //  so the 'complete' status doesn't fire.
    // -iframe2.html does a setTimeout to navigate itself to iframe3.html. This
    // allows the page to stop loading and the 'complete' status to fire, but
    // when the timeout fires, the pages goes back into the loading state
    // which causes the new status: 'loading' event to fire without having
    // changed the url.
    expect([
      { status: 'loading', url: getURL('iframeNavigated/a.html') },
      { status: 'complete' },
      { status: 'loading' },
      { status: 'complete' },
    ]);

    chrome.tabs.create({ url: getURL('iframeNavigated/a.html') });
  },

  function internalAnchorNavigated() {
    expect([
      { status: 'loading', url: getURL('internalAnchorNavigated/a.html') },
      { status: 'complete' },
      { status: 'loading', url: getURL('internalAnchorNavigated/a.html#b') },
      { status: 'complete' },
    ]);

    chrome.tabs.create({ url: getURL('internalAnchorNavigated/a.html') });
  },

  function faviconLoaded() {
    expect([
      { status: 'loading', url: getURL('favicon/a.html') },
      { status: 'complete' },
      { favIconUrl: getURL('favicon/favicon.ico') },
    ]);

    chrome.tabs.create({ url: getURL('favicon/a.html') });
  },

  function titleUpdated() {
    expect([
      { status: 'loading', url: getURL('title/test.html') },
      { status: 'complete' },
      { title: 'foo' },
      { title: 'bar' }
    ]);

    chrome.tabs.create({ url: getURL('title/test.html') });
  }
]);
