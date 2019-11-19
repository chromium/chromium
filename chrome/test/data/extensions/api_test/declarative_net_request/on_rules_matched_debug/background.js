// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Navigates to |url| and invokes |callback| when the navigation is complete.
function navigateTab(url, expectedTabUrl, callback) {
  chrome.tabs.onUpdated.addListener(function updateCallback(_, info, tab) {
    if (info.status == 'complete' && tab.url == expectedTabUrl) {
      chrome.tabs.onUpdated.removeListener(updateCallback);
      callback(tab);
    }
  });

  chrome.tabs.update({url: url});
}

var matchedRules = [];
var onRuleMatchedDebugCallback = (rule) => {
  matchedRules.push(rule);
};

var testServerPort;
function getServerURL(host) {
  if (!testServerPort)
    throw new Error('Called getServerURL outside of runTests.');
  return `http://${host}:${testServerPort}/`;
}

function addRuleMatchedListener() {
  chrome.declarativeNetRequest.onRuleMatchedDebug.addListener(
      onRuleMatchedDebugCallback);
}

function removeRuleMatchedListener() {
  matchedRules = [];
  chrome.declarativeNetRequest.onRuleMatchedDebug.removeListener(
      onRuleMatchedDebugCallback);
}

var tests = [
  function testBlockRule() {
    addRuleMatchedListener();
    const url = getServerURL('abc.com');
    navigateTab(url, url, (tab) => {
      chrome.test.assertEq(1, matchedRules.length);
      const matchedRule = matchedRules[0];

      // The request ID may not be known but should be populated.
      chrome.test.assertTrue(matchedRule.request.hasOwnProperty('requestId'));
      delete matchedRule.request.requestId;

      const expectedRuleInfo = {
        request: {
          initiator: `chrome-extension://${chrome.runtime.id}`,
          method: 'GET',
          frameId: 0,
          parentFrameId: -1,
          tabId: tab.id,
          type: 'main_frame',
          url: `http://abc.com:${testServerPort}/`
        },
        rule: {ruleId: 1, sourceType: 'manifest'}
      };

      // Sanity check that the MatchedRuleInfoDebug fields are populated
      // correctly.
      chrome.test.assertTrue(
          chrome.test.checkDeepEq(expectedRuleInfo, matchedRule));

      removeRuleMatchedListener();
      chrome.test.succeed();
    });
  },

  function testNoRuleMatched() {
    addRuleMatchedListener();
    const url = getServerURL('nomatch.com');
    navigateTab(url, url, (tab) => {
      chrome.test.assertEq(0, matchedRules.length);
      removeRuleMatchedListener();
      chrome.test.succeed();
    });
  },

  function testAllowRule() {
    addRuleMatchedListener();

    const url = getServerURL('abcde.com');
    navigateTab(url, url, (tab) => {
      // The allow rule should not be matched twice despite it overriding both
      // a block and a redirect rule (rules with id 1 and 5).
      chrome.test.assertEq(1, matchedRules.length);
      const matchedRule = matchedRules[0];
      chrome.test.assertEq(4, matchedRule.rule.ruleId);

      removeRuleMatchedListener();
      chrome.test.succeed();
    });
  },

  function testMultipleRules() {
    addRuleMatchedListener();

    // redir1.com --> redir2.com --> abc.com (blocked)
    // 3 rules are matched from the above sequence of actions.
    navigateTab(getServerURL('redir1.com'), 'http://abc.com/', (tab) => {
      chrome.test.assertEq(3, matchedRules.length);

      const expectedRuleIDs = [2, 3, 1];
      for (let i = 0; i < matchedRules.length; ++i)
        chrome.test.assertEq(expectedRuleIDs[i], matchedRules[i].rule.ruleId);

      removeRuleMatchedListener();
      chrome.test.succeed();
    });
  }
];

chrome.test.getConfig(function(config) {
  testServerPort = config.testServer.port;
  chrome.test.runTests(tests);
});
