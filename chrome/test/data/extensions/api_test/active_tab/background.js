// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const assertEq = chrome.test.assertEq;
const assertFalse = chrome.test.assertFalse;
const assertTrue = chrome.test.assertTrue;
const callbackFail = chrome.test.callbackFail;
const callbackPass = chrome.test.callbackPass;

const RoleType = chrome.automation.RoleType;

function canXhr(url) {
  assertFalse(url == null);
  const xhr = new XMLHttpRequest();
  xhr.open('GET', url, false);
  let success = true;
  try {
    xhr.send();
  } catch (e) {
    assertEq('NetworkError', e.name);
    success = false;
  }
  return success;
}

let cachedUrl = null;
let iframeDone = null;

chrome.runtime.onMessage.addListener(function(request, sender, sendResponse) {
  if (request.message == 'xhr') {
    sendResponse({url: cachedUrl});
  } else {
    assertTrue(request.success);
    iframeDone();
  }
});

const iframeUrl = chrome.runtime.getURL('iframe.html');
const injectIframe = `let iframe = document.createElement('iframe');\n` +
    `iframe.src = '${iframeUrl}';\n` +
    `document.body.appendChild(iframe);\n`;

let runCount = 0;
chrome.browserAction.onClicked.addListener(function(tab) {
  runCount++;
  if (runCount == 1) {
    // First pass is done without granting activeTab permission, the extension
    // shouldn't have access to tab.url here.
    assertFalse(!!tab.url);
    chrome.test.succeed();
    return;
  } else if (runCount == 3) {
    // Third pass is done in a public session, and activeTab permission is
    // granted to the extension. URL should be scrubbed down to the origin
    // here (tested at the C++ side).
    chrome.test.sendMessage(tab.url);
    chrome.test.succeed();
    return;
  }
  // Second pass is done with granting activeTab permission, the extension
  // should have full access to the page (and also to tab.url).

  iframeDone = chrome.test.callbackAdded();
  cachedUrl = tab.url;
  chrome.tabs.executeScript({code: injectIframe}, callbackPass());
  assertTrue(canXhr(tab.url));
});

let navigationCount = 0;
chrome.webNavigation.onCompleted.addListener(function(details) {
  if (!details.url.endsWith('page.html')) {
    return;
  }

  navigationCount++;
  chrome.test.sendMessage(navigationCount.toString());

  // The second navigation remains on the same site, so we should still have
  // access.
  const expectHasAccess = navigationCount === 2;

  if (expectHasAccess) {
    chrome.tabs.executeScript({code: 'true'}, callbackPass());
    chrome.automation.getDesktop(callbackPass());
    assertTrue(canXhr(details.url));
    return;
  }

  chrome.tabs.executeScript(
      {code: 'true'},
      callbackFail(
          'Cannot access contents of the page. ' +
          'Extension manifest must request permission to access the ' +
          'respective host.'));

  chrome.automation.getDesktop(
      callbackFail('Failed request of automation on a page'));

  assertFalse(canXhr(details.url));
});

chrome.test.sendMessage('ready');
