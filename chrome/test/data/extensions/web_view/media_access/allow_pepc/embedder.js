// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const embedder = {};
embedder.tests = {};
embedder.baseGuestURL = '';
embedder.guestURL = '';

embedder.failTest = function(msg) {
  window.console.warn(`test failure, reason: ${msg}`);
  chrome.test.sendMessage('TEST_FAILED');
};

embedder.maybePassTest = function() {
  chrome.test.sendMessage('TEST_PASSED');
};

embedder.setUpGuest_ = function() {
  document.querySelector('#webview-tag-container').innerHTML =
      `<webview style="width: 100px; height: 100px;" src="${
          embedder.guestURL}"></webview>`;
  const webview = document.querySelector('webview');
  if (!webview) {
    embedder.failTest('No <webview> element created');
    return null;
  }
  return webview;
};

embedder.setUpLoadStop_ = function(webview, testName) {
  const onWebViewLoadStop = function(e) {
    webview.contentWindow.postMessage(
        JSON.stringify(['check-media-permission', `${testName}`]), '*');
  };
  webview.addEventListener('loadstop', onWebViewLoadStop);
};

embedder.assertCorrectMediaEvent_ = function(e) {
  if (e.permission !== 'media') {
    embedder.failTest(`wrong permission: ${e.permission}`);
    return false;
  }
  if (!e.url || !e.request.url) {
    embedder.failTest('No url property in event');
    return false;
  }
  if (e.url.indexOf(embedder.baseGuestURL)) {
    embedder.failTest(`Wrong url: ${e.url}, expected url to start with ${
        embedder.baseGuestURL}`);
    return false;
  }

  return true;
};

embedder.tests.testAllowPEPC = function() {
  const webview = embedder.setUpGuest_();
  if (!webview) {
    return;
  }

  const onPermissionRequest = function(e) {
    if (!embedder.assertCorrectMediaEvent_(e)) {
      return;
    }
    e.request.allow();
  };
  webview.addEventListener('permissionrequest', onPermissionRequest);

  window.addEventListener('message', function(e) {
    try {
      const data = JSON.parse(e.data);
      if (data[0] === 'access-granted') {
        embedder.maybePassTest();
      } else {
        embedder.failTest(`Guest denied access: ${data[1]}`);
      }
    } catch (err) {
      // Ignore non-json messages
    }
  });

  embedder.setUpLoadStop_(webview, 'testAllowPEPC');
};

embedder.tests.list = {
  'testAllowPEPC': embedder.tests.testAllowPEPC,
};

function runTest(testName) {
  chrome.test.getConfig(function(config) {
    embedder.baseGuestURL = `http://localhost:${config.testServer.port}`;
    embedder.guestURL = `${embedder.baseGuestURL}/media_access_guest.html`;
    chrome.test.log(`Guest url is: ${embedder.guestURL}`);

    const testFunction = embedder.tests.list[testName];
    if (!testFunction) {
      embedder.failTest(`No such test: ${testName}`);
      return;
    }
    testFunction();
  });
}

onload = function() {
  chrome.test.sendMessage('LAUNCHED');
};
