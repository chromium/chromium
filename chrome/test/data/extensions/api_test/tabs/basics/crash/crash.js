// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const INDUCE_BROWSER_CRASH_URL = 'about:inducebrowsercrashforrealz';
const INDUCE_RENDERER_CRASH_URL = 'about:crash';
const ERROR = `I'm sorry. I'm afraid I can't do that.`;

const succeed = chrome.test.succeed;
const callbackFail = chrome.test.callbackFail;

const SCRIPT_URL = '_test_resources/api_test/tabs/basics/tabs_util.js';
const loadScript = chrome.test.loadScript(SCRIPT_URL);

loadScript.then(async function() {
  chrome.test.runTests([

    function crashBrowserTabsCreate() {
      chrome.tabs.create({url: INDUCE_BROWSER_CRASH_URL}, callbackFail(ERROR));
    },

    function crashBrowserWindowCreate() {
      chrome.windows.create(
          {url: INDUCE_BROWSER_CRASH_URL}, callbackFail(ERROR));
    },

    function crashBrowserWindowCreateArray() {
      const urls = ['about:blank', INDUCE_BROWSER_CRASH_URL];
      chrome.windows.create({url: urls}, callbackFail(ERROR));
    },

    function crashBrowserTabsUpdate() {
      chrome.tabs.create({url: 'about:blank'}, function(tab) {
        chrome.tabs.update(
            tab.id, {url: INDUCE_BROWSER_CRASH_URL}, callbackFail(ERROR));
      });
    },

    function crashRendererTabsCreate() {
      chrome.tabs.create({url: INDUCE_RENDERER_CRASH_URL}, callbackFail(ERROR));
    },

    function crashRendererWindowCreate() {
      chrome.windows.create(
          {url: INDUCE_RENDERER_CRASH_URL}, callbackFail(ERROR));
    },

    function crashRendererWindowCreateArray() {
      const urls = ['about:blank', INDUCE_RENDERER_CRASH_URL];
      chrome.windows.create({url: urls}, callbackFail(ERROR));
    },

    function crashRendererTabsUpdate() {
      chrome.tabs.create({url: 'about:blank'}, function(tab) {
        chrome.tabs.update(
            tab.id, {url: INDUCE_RENDERER_CRASH_URL}, callbackFail(ERROR));
      });
    },

  ]);
});
