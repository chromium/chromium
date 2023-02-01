// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var INDUCE_BROWSER_CRASH_URL = 'about:inducebrowsercrashforrealz';
var INDUCE_RENDERER_CRASH_URL = 'about:crash';
var ERROR = "I'm sorry. I'm afraid I can't do that.";

var succeed = chrome.test.succeed;
var callbackFail = chrome.test.callbackFail;

const scriptUrl = '_test_resources/api_test/tabs/basics/tabs_util.js';
let loadScript = chrome.test.loadScript(scriptUrl);

loadScript.then(async function() {
chrome.test.runTests([

  function crashBrowserTabsCreate() {
    chrome.tabs.create({url: INDUCE_BROWSER_CRASH_URL}, callbackFail(ERROR));
  },

  function crashBrowserWindowCreate() {
    chrome.windows.create({url: INDUCE_BROWSER_CRASH_URL}, callbackFail(ERROR));
  },

  function crashBrowserWindowCreateArray() {
    var urls = ['about:blank', INDUCE_BROWSER_CRASH_URL];
    chrome.windows.create({url: urls}, callbackFail(ERROR));
  },

  function crashBrowserTabsUpdate() {
    chrome.tabs.create({url: 'about:blank'}, function(tab) {
      chrome.tabs.update(tab.id,
                         {url: INDUCE_BROWSER_CRASH_URL},
                         callbackFail(ERROR));
    });
  },

  function crashRendererTabsCreate() {
    chrome.tabs.create({url: INDUCE_RENDERER_CRASH_URL}, callbackFail(ERROR));
  },

  function crashRendererWindowCreate() {
    chrome.windows.create({url: INDUCE_RENDERER_CRASH_URL},
                          callbackFail(ERROR));
  },

  function crashRendererWindowCreateArray() {
    var urls = ['about:blank', INDUCE_RENDERER_CRASH_URL];
    chrome.windows.create({url: urls}, callbackFail(ERROR));
  },

  function crashRendererTabsUpdate() {
    chrome.tabs.create({url: 'about:blank'}, function(tab) {
      chrome.tabs.update(
        tab.id, {url: INDUCE_RENDERER_CRASH_URL}, callbackFail(ERROR));
    });
  }

])});
