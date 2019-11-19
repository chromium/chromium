// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function assertNoSensitiveFields(tab) {
  ['url', 'pendingUrl', 'title', 'favIconUrl'].forEach(function(field) {
    chrome.test.assertEq(undefined, tab[field],
                         'Sensitive property ' + field + ' is visible')
  });
}

var port;

function testUrl(domain, file) {
    return 'http://' + domain + ':' + port +
      '/extensions/favicon/' + file;
}

function makeCallbackAfterLoaded(expectedTabId, needsFavIcon, callback) {
  chrome.tabs.onUpdated.addListener(function _listener(tabId, info, tab) {
    if (expectedTabId == tabId && tab.status == 'complete' &&
        (tab.favIconUrl || !needsFavIcon)) {
      chrome.tabs.onUpdated.removeListener(_listener);
      callback();
    }
  });
}

var pass = chrome.test.callbackPass;

chrome.test.getConfig(function(config) {
  port = config.testServer.port;

  const HAS_PERMISSION_URL = testUrl('a.com', 'test_file.html');
  const NO_PERMISSION_URL = testUrl('b.com', 'test_file.html');
  const FAV_ICON_URL = testUrl('a.com', 'favicon.ico');

  chrome.test.runTests([
    function testSimpleCreateWithHostPermission() {
      chrome.tabs.create({url: HAS_PERMISSION_URL}, function (tab) {
        chrome.test.assertEq(HAS_PERMISSION_URL, tab.pendingUrl);
        chrome.test.assertEq(undefined, tab.url);

        makeCallbackAfterLoaded(tab.id, true, function() {
          chrome.tabs.get(tab.id, function(tab) {
            chrome.test.assertEq(undefined, tab.pendingUrl);
            chrome.test.assertEq(HAS_PERMISSION_URL, tab.url);
            chrome.test.assertEq('Title', tab.title);
            chrome.test.assertEq(FAV_ICON_URL, tab.favIconUrl);
            chrome.test.succeed();
          });
        });
      });
    },

    function testUpdateToAndFromHostPermissions() {
      // On creation of a page we have host permissions to, we should be able to
      // see the pendingUrl, but no other sensitive properties.
      var assertPropertiesOnCreated = function(tab) {
        chrome.test.assertEq(HAS_PERMISSION_URL, tab.pendingUrl);
        chrome.test.assertEq(undefined, tab.url);
        chrome.test.assertEq(undefined, tab.title);
        chrome.test.assertEq(undefined, tab.favIconUrl);
      };
      // Updating away from a page we have host permissions to, we shouldn't
      // get the pendingUrl, but we will still be on the context of the previous
      // page at the time of callback, so will get that info still.
      var assertPropertiesOnUpdatingAwayFromPermission = function(tab) {
        chrome.test.assertEq(undefined, tab.pendingUrl);
        chrome.test.assertEq(HAS_PERMISSION_URL, tab.url);
        chrome.test.assertEq('Title', tab.title);
        chrome.test.assertEq(FAV_ICON_URL, tab.favIconUrl);
      };
      // Updating from a page we do not have host permissions to, to a page we
      // do have host permissions to, we should see the pendingUrl, but none of
      // the previous page details.
      var assertPropertiesOnUpdatingBackToPermission = function(tab) {
        chrome.test.assertEq(HAS_PERMISSION_URL, tab.pendingUrl);
        chrome.test.assertEq(undefined, tab.url);
        chrome.test.assertEq(undefined, tab.title);
        chrome.test.assertEq(undefined, tab.favIconUrl);
      };

      // These checks each rely on the previous navigation having completed, to
      // check for the correct values for favIconUrl, title and url.
      chrome.tabs.create({url: HAS_PERMISSION_URL}, function(tab) {
        assertPropertiesOnCreated(tab);
        makeCallbackAfterLoaded(tab.id, true, function() {
          chrome.tabs.update({url: NO_PERMISSION_URL}, function(tab) {
            assertPropertiesOnUpdatingAwayFromPermission(tab);
            makeCallbackAfterLoaded(tab.id, false, function() {
              chrome.tabs.update({url: HAS_PERMISSION_URL}, function(tab) {
                assertPropertiesOnUpdatingBackToPermission(tab);
                chrome.test.succeed();
              });
            });
          });
        });
      });
    },

    function testOnUpdatedRevealsNoSensitiveFieldsWithNoHostPermission() {
      var getCurrentTabs = new Promise(function(resolve) {
        chrome.tabs.query({}, (tabs) => {
          resolve(tabs);
        });
      });

      getCurrentTabs.then((existingTabs) => {
        var neededCallbacks = 2;
        var existingTabIds = existingTabs.map(tab => tab.id);
        chrome.tabs.onUpdated.addListener(function _listener(tabId, info, tab) {
          if (existingTabIds.includes(tabId))
            return; // Ignore tabs that were already around.
          assertNoSensitiveFields(info);
          assertNoSensitiveFields(tab);
          neededCallbacks--;
          if (neededCallbacks == 0) {
            chrome.tabs.onUpdated.removeListener(_listener);
            chrome.test.succeed();
          }
        });

        chrome.tabs.create({url: 'chrome://newtab/'}, function(tab) {
          assertNoSensitiveFields(tab);
          chrome.tabs.update(tab.id, {url: 'about:blank'}, function(tab) {
            assertNoSensitiveFields(tab);
          });
        });
      });
    },

    function testQueryRevealsNoSensitiveFieldsWithNoHostPermission() {
      chrome.tabs.create({url: 'chrome://newtab/'}, pass(function(tab) {
        assertNoSensitiveFields(tab);
      }));
      chrome.tabs.query({active: true}, pass(function(tabs) {
        chrome.test.assertEq(1, tabs.length);
        assertNoSensitiveFields(tabs[0]);
      }));
    }
  ]);
});
