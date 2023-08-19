// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var assertEq = chrome.test.assertEq;
var assertFalse = chrome.test.assertFalse;
var assertTrue = chrome.test.assertTrue;

var EventType = chrome.automation.EventType;
var RoleType = chrome.automation.RoleType;
var StateType = chrome.automation.StateType;

var rootNode = null;
var url = '';

function listenOnce(node, eventType, callback, capture) {
  var innerCallback = function(evt) {
    node.removeEventListener(eventType, innerCallback, capture);
    callback(evt);
  };
  node.addEventListener(eventType, innerCallback, capture);
}

function setUpAndRunTests(allTests, opt_path) {
  var path = opt_path || 'index.html';
  getUrlFromConfig(path, function(url) {
    import('/_test_resources/test_util/tabs_util.js').then(async (tabUtil) => {
      await tabUtil.openTab(url);
      chrome.automation.getDesktop(function(desktop) {
        rootNode = desktop.find({attributes: {docUrl: url}});
        if (rootNode && rootNode.docLoaded) {
          chrome.test.runTests(allTests);
          return;
        }
        let listener = () => {
        rootNode = desktop.find({attributes: {docUrl: url}});
          if (rootNode && rootNode.docLoaded) {
            desktop.removeEventListener('loadComplete', listener);
          desktop.addEventListener('focus', () => {});
            chrome.test.runTests(allTests);
          }
        };
        desktop.addEventListener('loadComplete', listener);
    });});
  });
}

function getUrlFromConfig(path, callback) {
  chrome.test.getConfig(function(config) {
    assertTrue('testServer' in config, 'Expected testServer in config');
    url = ('http://a.com:PORT/' + path)
        .replace(/PORT/, config.testServer.port);
    callback(url)
  });
}

function createTabAndWaitUntilLoaded(url, callback) {
  chrome.tabs.create({'url': url}, function(tab) {
    chrome.tabs.onUpdated.addListener(function(tabId, changeInfo) {
      if (tabId == tab.id && changeInfo.status == 'complete') {
        callback(tab);
      }
    });
  });
}
