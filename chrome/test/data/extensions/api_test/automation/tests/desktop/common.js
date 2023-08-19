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

function findAutomationNode(root, condition) {
  if (condition(root))
    return root;

  var children = root.children;
  for (var i = 0; i < children.length; i++) {
    var result = findAutomationNode(children[i], condition);
    if (result)
      return result;
  }
  return null;
}

function runWithDocument(docString, callback) {
  var url = 'data:text/html,<!doctype html>' + docString;
  var createParams = {
    active: true,
    url: url
  };
  createTabAndWaitUntilLoaded(url, function(tab) {
    chrome.automation.getDesktop(desktop => {
      const url = tab.url || tab.pendingUrl;
      let rootNode = desktop.find({attributes: {docUrl: url}});
      if (rootNode && rootNode.docLoaded) {
        callback(rootNode);
        return;
      }

      let listener = () => {
        rootNode = desktop.find({attributes: {docUrl: url}});
        if (rootNode && rootNode.docLoaded) {
          desktop.removeEventListener('loadComplete', listener);
          desktop.addEventListener('focus', () => {});
          callback(rootNode);
        }
      };
      desktop.addEventListener('loadComplete', listener);
    });
  });
}

function listenOnce(node, eventType, callback, capture) {
  var innerCallback = function(evt) {
    node.removeEventListener(eventType, innerCallback, capture);
    callback(evt);
  };
  node.addEventListener(eventType, innerCallback, capture);
}

function setUpAndRunTests(allTests) {
  chrome.automation.getDesktop(function(rootNodeArg) {
    rootNode = rootNodeArg;
    chrome.test.runTests(allTests);
  });
}

function setUpAndRunTestsInPage(allTests, opt_path, opt_ensurePersists = true) {
  var path = opt_path || 'index.html';
  getUrlFromConfig(path, function(url) {
    createTabAndWaitUntilLoaded(url, function(unused_tab) {
      chrome.automation.getDesktop(function(desktop) {
        rootNode = desktop.find({attributes: {docUrl: url}});
        if (rootNode && rootNode.docLoaded) {
          chrome.test.runTests(allTests);
          return;
        }
        function listener() {
          rootNode = desktop.find({attributes: {docUrl: url}});
          if (rootNode && rootNode.docLoaded) {
            desktop.removeEventListener('loadComplete', listener);
            if (opt_ensurePersists) {
              desktop.addEventListener('focus', () => {});
            }
            chrome.test.runTests(allTests);
          }
        }
        desktop.addEventListener('loadComplete', listener);
      });
    });
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
  chrome.tabs.create({"url": url}, function(tab) {
    chrome.tabs.onUpdated.addListener(function(tabId, changeInfo) {
      if (tabId == tab.id && changeInfo.status == 'complete') {
        callback(tab);
      }
    });
  });
}

async function pollUntil(predicate, pollEveryMs) {
  return new Promise(r => {
    const id = setInterval(() => {
      let ret;
      if (ret = predicate()) {
        clearInterval(id);
        r(ret);
      }
    }, pollEveryMs);
  });
}
