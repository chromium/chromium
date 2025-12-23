// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var firstWindowId;

const scriptUrl = '_test_resources/api_test/tabs/basics/tabs_util.js';
let loadScript = chrome.test.loadScript(scriptUrl);

let isAndroid;

// Tests that the given `url` and `title` are appropriate values for the NTP.
// This has subtly different behavior on different platforms.
function smellsLikeNewTabPage(url, title) {
  const ntpUrls = [
      'chrome://newtab/',
      'chrome-native://newtab/',
  ];
  chrome.test.assertTrue(ntpUrls.includes(url),
                         `Unexpected URL: ${url}`);
  if (!isAndroid) {
    // For most create calls the title will be an empty string until the
    // navigation commits, but the new tab page is an exception to this.
    // However, this isn't true on Android.
    chrome.test.assertEq('New Tab', title);
  }
}

function getSelectedAdapter(winId, callback) {
  const manifest = chrome.runtime.getManifest();
  if (manifest.manifest_version < 3) {
    chrome.tabs.getSelected(winId, callback);
    return;
  }

  chrome.test.assertEq(3, manifest.manifest_version);
  chrome.tabs.query({windowId:winId, active:true}, tabs => {
    if (tabs.length == 0) {
      callback(undefined);
    } else {
      callback(tabs[0]);
    }
  });
}

const tests = [
  function getSelected() {
    getSelectedAdapter(null, pass(function(tab) {
      assertEq('about:blank', tab.url);
      assertEq('about:blank', tab.title);
      firstWindowId = tab.windowId;
    }));
  },

  function create() {
    chrome.tabs.create({"windowId" : firstWindowId, "active" : false},
                       pass(function(tab){
      assertEq(1, tab.index);
      assertEq(firstWindowId, tab.windowId);
      assertEq(false, tab.selected);
      smellsLikeNewTabPage(tab.pendingUrl, tab.title);
      assertEq("", tab.url);
      assertEq(false, tab.pinned);
      waitForAllTabs(pass(function() {
        chrome.tabs.get(tab.id, pass(function(tab) {
          smellsLikeNewTabPage(tab.url, tab.title);
          assertEq(undefined, tab.pendingUrl);
        }));
      }));
    }));
  },

  function createInCurrent() {
    chrome.tabs.create({'windowId': chrome.windows.WINDOW_ID_CURRENT},
                       pass(function(tab) {
      chrome.windows.getCurrent(pass(function(win) {
        assertEq(win.id, tab.windowId);
      }));
    }));
  },

  function createInOtherWindow() {
    chrome.windows.create({}, pass(function(win) {
      // Create a tab in the older window.
      chrome.tabs.create({"windowId" : firstWindowId, "active" : false},
                         pass(function(tab) {
        assertEq(firstWindowId, tab.windowId);
      }));
      // Create a tab in this new window.
      chrome.tabs.create({"windowId" : win.id}, pass(function(tab) {
        assertEq(win.id, tab.windowId);
      }));
    }));
  },

  function createAtIndex() {
    chrome.tabs.create({"windowId" : firstWindowId, "index" : 1},
                       pass(function(tab) {
      assertEq(1, tab.index);
    }));
  },

  function createSelected() {
    chrome.tabs.create({"windowId" : firstWindowId, "active" : true},
                       pass(function(tab) {
      assertTrue(tab.active && tab.selected);
      getSelectedAdapter(firstWindowId, pass(function(selectedTab) {
        assertEq(tab.id, selectedTab.id);
      }));
    }));
  },

  function createWindowWithDefaultTab() {
    var verify_default = function() {
      return pass(function(win) {
        assertEq(1, win.tabs.length);
        // In case the URL has or has not committed yet, check both.
        const url = win.tabs[0].pendingUrl || win.tabs[0].url;
        smellsLikeNewTabPage(url, win.tabs[0].title);
      });
    };

    // Make sure the window always has the NTP when no URL is supplied.
    chrome.windows.create({}, verify_default());
    chrome.windows.create({url:[]}, verify_default());
  },

  function createWindowWithExistingTab() {
    // Create a tab in the old window
    chrome.tabs.create({"windowId" : firstWindowId, "url": pageUrl('a'),
                        "active" : false},
                       pass(function(tab) {
      assertEq(firstWindowId, tab.windowId);
      assertEq(pageUrl('a'), tab.pendingUrl);

      // Create a new window with this tab
      chrome.windows.create({"tabId": tab.id}, pass(function(win) {
        assertEq(1, win.tabs.length);
        assertEq(tab.id, win.tabs[0].id);
        assertEq(win.id, win.tabs[0].windowId);
        // In case the URL has or has not committed yet, check both.
        const url = win.tabs[0].pendingUrl || win.tabs[0].url;
        assertEq(pageUrl('a'), url);
      }));
    }));
  },

  function windowCreate() {
    chrome.windows.create({type: "popup"}, pass(function(window) {
      assertEq("popup", window.type);
      assertTrue(!window.incognito);
    }));
    chrome.windows.create({incognito: true}, pass(function(window) {
      // This extension is not incognito-enabled, so it shouldn't be able to
      // see the incognito window.
      assertEq(null, window);
    }));
  },
];

// The following tests don't work on desktop android (yet).
// TODO(https://crbug.com/371432155): Enable these on desktop android.
const skipForAndroid = [
    'createAtIndex',
    'createWindowWithDefaultTab',
    'createWindowWithExistingTab',
    'windowCreate',
];

(async function() {
  const os = await new Promise((resolve) => {
    chrome.runtime.getPlatformInfo(info => resolve(info.os));
  });
  isAndroid = os === 'android';
  let testsToRun = tests;
  if (isAndroid) {
    testsToRun =
        tests.filter((t) => { return !skipForAndroid.includes(t.name); });
  }

  await loadScript;

  chrome.test.runTests(testsToRun);
})();
