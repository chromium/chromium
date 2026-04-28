// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const server = 'http://b.com:PORT';
const RELATIVE_PATH = '/extensions/api_test/executescript/file_after_close/';
const extensionPage = chrome.runtime.getURL('extension_page.html');
let webPage1 = `${server}${RELATIVE_PATH}web_page1.html`;
let webPage2 = `${server}${RELATIVE_PATH}web_page2.html`;
let extensionPageOpened = false;

const listener = function(tabId, changeInfo, tab) {
  if (changeInfo.status != 'complete') {
    return;
  }

  // web_page1 loaded, open extension page to inject script
  if (!extensionPageOpened && tab.url == webPage1) {
    chrome.tabs.create({ url: extensionPage });
    extensionPageOpened = true;
    return;
  }

  if (tab.url == webPage2) {
    console.log('webPage1 navigated to webPage1. Yeah!');
    chrome.tabs.onUpdated.removeListener(listener);
    chrome.test.notifyPass();
  }
};

chrome.tabs.onUpdated.addListener(listener);
chrome.test.getConfig(function(config) {
  webPage1 = webPage1.replace(/PORT/, config.testServer.port);
  webPage2 = webPage2.replace(/PORT/, config.testServer.port);
  chrome.tabs.create({ url: webPage1 });
});

