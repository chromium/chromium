// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// browser_tests --gtest_filter=ExtensionApiTest.OpenOptionsPage

const assertEq = chrome.test.assertEq;
const assertTrue = chrome.test.assertTrue;
const listenOnce = chrome.test.listenOnce;
const callbackPass = chrome.test.callbackPass;

let optionsTabUrl = `chrome://extensions/?options=${chrome.runtime.id}`;

// Finds the Tab for an options page, or null if no options page is open.
// Asserts that there is at most 1 options page open.
// Result is passed to |callback|.
function findOptionsTab(callback) {
  chrome.runtime.getPlatformInfo(function(info) {
    if (info.os === 'android') {
      // The options page on Android has a different URL in the form of
      // chrome-extensions://<extension-id>/options.html, so use that
      // one instead.
      optionsTabUrl = chrome.runtime.getURL('options.html');
    }
    chrome.tabs.query({url: optionsTabUrl}, callbackPass(function(tabs) {
                        assertTrue(tabs.length <= 1);
                        callback(tabs.length == 0 ? null : tabs[0]);
                      }));
  });
}

// Tests opening a new options page.
function testNewOptionsPage() {
  findOptionsTab(function(tab) {
    assertEq(null, tab);
    listenOnce(chrome.runtime.onMessage, function(m, sender) {
      assertEq('success', m);
      assertEq(chrome.runtime.id, sender.id);
      assertEq(chrome.runtime.getURL('options.html'), sender.url);
    });
    chrome.runtime.openOptionsPage(callbackPass());
  });
}

// Gets the active tab, or null if no tab is active. Asserts that there is at
// most 1 active tab. Result is passed to |callback|.
function getActiveTab(callback) {
  chrome.tabs.query({active: true}, callbackPass(function(tabs) {
                      assertTrue(tabs.length <= 1);
                      callback(tabs.length == 0 ? null : tabs[0]);
                    }));
}

// Tests refocusing an existing page.
function testRefocusExistingOptionsPage() {
  const testUrl = 'about:blank';

  // There will already be an options page open from the last test. Find it,
  // focus away from it, then make sure openOptionsPage() refocuses it.
  findOptionsTab(function(optionsTab) {
    assertTrue(optionsTab != null);
    chrome.tabs.create(
        {url: testUrl}, callbackPass(function(tab) {
          // Make sure the new tab is active.
          getActiveTab(async function(activeTab) {
            assertEq(testUrl, activeTab.url || activeTab.pendingUrl);

            // Desktop Android does not support refocusing the options page,
            // as the option page cannot be embedded as a guest view like it is
            // on other platforms.
            // See:
            // https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/extensions/extension_tab_util.cc;l=1165-1175;drc=3c2a50b10dbf0b02620aae4d69f80fcf1061292e
            // and
            // https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/extensions/extension_tab_util.cc;l=207-215;drc=eb1b4c6a26bdb372fc55844b73f5b21a0f38b674
            if ((await chrome.runtime.getPlatformInfo()).os === 'android') {
              chrome.test.succeed('skipped');
              return;
            }

            // On Win/Mac/Linux, open options page should refocus it
            // immediately.
            chrome.runtime.openOptionsPage(callbackPass(function() {
              getActiveTab(function(activeTab) {
                assertEq(optionsTabUrl, activeTab.url);
              });
            }));
          });
        }));
  });
}

chrome.test.runTests([testNewOptionsPage, testRefocusExistingOptionsPage]);
