// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var tabProps = [];

var createTabUtil = function(urlToLoad, createdCallback) {
  try {
    chrome.tabs.create({url: urlToLoad}, function(tab) {
      createdCallback({id: tab.id, url: tab.pendingUrl});
    });
  } catch (e) {
    chrome.test.fail(e);
  }
}

var getTabUtil = function(tabId, getCallback) {
  try {
    chrome.tabs.get(tabId, function(tab) {
      getCallback({id: tab.id, url: tab.pendingUrl || tab.url});
    });
  } catch (e) {
    chrome.test.fail(e);
  }
}

var queryTabUtil = function(queryProps, queryCallback) {
  try {
    chrome.tabs.query(queryProps, queryCallback);
  } catch(e) {
    chrome.test.fail(e);
  }
}

chrome.test.runTests([
  // Get the info for the tab that was automatically created.
  function testTabQueryInitial() {
    queryTabUtil({currentWindow: true}, function(tabs) {
      chrome.test.assertEq(1, tabs.length);
      tabProps.push({id: tabs[0].id, url: tabs[0].url});
      chrome.test.succeed();
    });
  },
  // Create a new tab.
  function testTabCreate1() {
    var expectedUrl = 'chrome://version/';
    createTabUtil(expectedUrl, function(tabData) {
      chrome.test.assertEq(expectedUrl, tabData.url);
      tabProps.push(tabData);
      chrome.test.succeed();
    });
  },
  // Check that it exists.
  function testTabGetAfterCreate1() {
    var expectedId = tabProps[tabProps.length - 1].id;
    var expectedUrl = tabProps[tabProps.length - 1].url;
    getTabUtil(expectedId, function(tabData) {
      chrome.test.assertEq(expectedId, tabData.id);
      chrome.test.assertEq(expectedUrl, tabData.url);
      chrome.test.succeed();
    });
  },
  // Create another new tab.
  function testTabCreate2() {
    var expectedUrl = 'chrome://version/';
    createTabUtil(expectedUrl, function(tabData) {
      chrome.test.assertEq(expectedUrl, tabData.url);
      tabProps.push(tabData);
      chrome.test.succeed();
    });
  },
  // Check that it also exists.
  function testTabGetAfterCreate2() {
    var expectedId = tabProps[tabProps.length - 1].id;
    var expectedUrl = tabProps[tabProps.length - 1].url;
    getTabUtil(expectedId, function(tabData) {
      chrome.test.assertEq(expectedId, tabData.id);
      chrome.test.assertEq(expectedUrl, tabData.url);
      chrome.test.succeed();
    });
  },
  // Verify that chrome.tabs.getCurrent doesn't work with background
  // pages.
  function testTabGetCurrent() {
    try {
      chrome.tabs.getCurrent(function(tab) {
        chrome.test.assertEq('undefined', typeof(tab));
        chrome.test.succeed();
      });
    } catch (e) {
      chrome.test.fail(e);
    }
  },
  // Duplicate the first tab created.
  function testTabGetDuplicate() {
    try {
      chrome.tabs.duplicate(tabProps[0].id, function(tab) {
        chrome.test.assertEq(tabProps[0].url, tab.url);
        tabProps.push({id: tab.id, url: tab.url});
        chrome.test.succeed();
      })
    } catch (e) {
      chrome.test.fail(e);
    }
  },
  // Check that the duplicate exists.
  function testTabGet3() {
    var expectedId = tabProps[tabProps.length - 1].id;
    var expectedUrl = tabProps[tabProps.length - 1].url;
    getTabUtil(expectedId, function(tabData) {
      chrome.test.assertEq(expectedId, tabData.id);
      chrome.test.assertEq(expectedUrl, tabData.url);
      chrome.test.succeed();
    });
  },
  // Query all the tabs and check their IDs and URLs are what
  // we expect.
  function testTabQuery2() {
    queryTabUtil({currentWindow: true}, function(tabs) {
      chrome.test.assertEq(tabProps.length, tabs.length);
      var countFound = 0;
      // This loop works because tab IDs are unique.
      for (var i = 0; i < tabs.length; ++i) {
        for (var j = 0; j < tabProps.length; ++j) {
          // Get the URL of the tab, which may still be pending.
          var tabUrl = tabs[i].pendingUrl || tabs[i].url;
          if (tabs[i].id === tabProps[j].id &&
              tabUrl === tabProps[j].url) {
            ++countFound;
            break;
          }
        }
      }
      chrome.test.assertEq(tabProps.length, countFound);
      chrome.test.succeed();
    });
  },
  // Remove all but the original tab. Removing them all will shut down the
  // browser, which we don't want.
  function testTabRemove() {
    try {
      var tabIds = [];
      for (var i = 1; i < tabProps.length; ++i) {
        tabIds.push(tabProps[i].id);
      }
      chrome.tabs.remove(tabIds, function() {
        chrome.test.succeed();
      });
    } catch(e) {
      chrome.test.fail(e);
    }
  },
  // Check that there's only one remaining tab.
  function testTabQuery3() {
    queryTabUtil({currentWindow: true}, function(tabs) {
      chrome.test.assertEq(1, tabs.length);
      chrome.test.assertEq(tabs[0].id, tabProps[0].id);
      chrome.test.assertEq(tabs[0].url, tabProps[0].url);
      chrome.test.succeed();
    });
  },
]);
