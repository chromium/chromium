// Copyright 2019 The Chromium Authors
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

function resetMatchedRules() {
  matchedRules = [];
}

function verifyExpectedRuleInfo(expectedRuleInfo) {
  chrome.test.assertEq(1, matchedRules.length);
  const matchedRule = matchedRules[0];

  // The request ID may not be known but should be populated.
  chrome.test.assertTrue(matchedRule.request.hasOwnProperty('requestId'));
  delete matchedRule.request.requestId;
  chrome.test.assertFalse(matchedRule.request.hasOwnProperty('documentId'));

  // Remove frame/parentFrameId/parentDocumentId if not set in the
  // expectedRules. The values of these fields vary depending on what type of
  // extension ContextType is used.
  if (!('frameId' in expectedRuleInfo.request)) {
    delete matchedRule.request.frameId;
  }
  if (!('parentFrameId' in expectedRuleInfo.request)) {
    delete matchedRule.request.parentFrameId;
  }
  if (!('parentDocumentId' in expectedRuleInfo.request)) {
    delete matchedRule.request.parentDocumentId;
  }

  chrome.test.assertEq(expectedRuleInfo, matchedRule);
}

var tests = [
  function setup() {
    chrome.declarativeNetRequest.onRuleMatchedDebug.addListener(
        onRuleMatchedDebugCallback);

    // This test was known to flake as reported in crbug.com/1029233 due to a
    // race condition where a request was sent and a declarative rule was
    // matched before the onRuleMatchedDebug listener was properly added.
    // Sending the request after waiting for a round trip should fix the
    // flakiness.
    chrome.test.waitForRoundTrip('msg', chrome.test.succeed);
  },

  function testDynamicRule() {
    resetMatchedRules();

    const rule = {
      id: 1,
      priority: 1,
      condition: {urlFilter: 'def', 'resourceTypes': ['main_frame']},
      action: {type: 'block'},
    };

    chrome.declarativeNetRequest.updateDynamicRules(
        {addRules: [rule]}, function() {
          chrome.test.assertNoLastError();
          const url = getServerURL('def.com');
          navigateTab(url, url, (tab) => {
            const expectedRuleInfo = {
              request: {
                initiator: `chrome-extension://${chrome.runtime.id}`,
                method: 'GET',
                documentLifecycle: 'active',
                frameType: 'outermost_frame',
                frameId: 0,
                parentFrameId: -1,
                tabId: tab.id,
                type: 'main_frame',
                url: url
              },
              rule: {
                ruleId: 1,
                rulesetId: chrome.declarativeNetRequest.DYNAMIC_RULESET_ID
              }
            };
            verifyExpectedRuleInfo(expectedRuleInfo);
            chrome.test.succeed();
          });
        });
  },

  function testBlockRule() {
    resetMatchedRules();

    const url = getServerURL('abc.com');
    navigateTab(url, url, (tab) => {
      const expectedRuleInfo = {
        request: {
          initiator: `chrome-extension://${chrome.runtime.id}`,
          method: 'GET',
          documentLifecycle: 'active',
          frameType: 'outermost_frame',
          frameId: 0,
          parentFrameId: -1,
          tabId: tab.id,
          type: 'main_frame',
          url: url
        },
        rule: {ruleId: 1, rulesetId: 'rules1'}
      };
      verifyExpectedRuleInfo(expectedRuleInfo);
      chrome.test.succeed();
    });
  },

  // Ensure that requests that don't originate from a tab (such as those from
  // the extension background page) trigger the listener.
  function testBackgroundPageRequest() {
    function listenOnce(target, callback) {
      let innerCallback = function(info) {
        target.removeListener(innerCallback);
        callback(info);
      };
      target.addListener(innerCallback);
    }

    listenOnce(
        chrome.declarativeNetRequest.onRuleMatchedDebug,
        function(fetchPromise, info) {
          fetchPromise
              .then(function(response) {
                chrome.test.fail('Request should be blocked by rule with ID 1');
              })
              .catch(function(error) {
                chrome.test.assertEq(1, info.rule.ruleId);
                chrome.test.assertEq('rules1', info.rule.rulesetId);

                // Tab ID should be -1 since this request was made from a
                // background page.
                chrome.test.assertEq(-1, info.request.tabId);
                chrome.test.succeed();
              });
        }.bind(null, fetch(getServerURL('abc.com'), {method: 'GET'})));
  },

  function testNoRuleMatched() {
    resetMatchedRules();
    const url = getServerURL('nomatch.com');
    navigateTab(url, url, (tab) => {
      chrome.test.assertEq(0, matchedRules.length);
      chrome.test.succeed();
    });
  },

  function testAllowRule() {
    resetMatchedRules();

    const url = getServerURL('abcde.com');
    navigateTab(url, url, (tab) => {
      // The allow rule should not be matched twice despite it overriding both
      // a block and a redirect rule (rules with id 1 and 5).
      chrome.test.assertEq(1, matchedRules.length);
      const matchedRule = matchedRules[0];
      chrome.test.assertEq(4, matchedRule.rule.ruleId);
      chrome.test.assertEq('rules2', matchedRule.rule.rulesetId);

      chrome.test.succeed();
    });
  },

  function testMultipleRules() {
    resetMatchedRules();

    // redir1.com --> redir2.com --> abc.com (blocked)
    // 3 rules are matched from the above sequence of actions.
    navigateTab(getServerURL('redir1.com'), 'http://abc.com/', (tab) => {
      chrome.test.assertEq(3, matchedRules.length);

      const expectedMatches = [
        {ruleId: 2, rulesetId: 'rules1'}, {ruleId: 3, rulesetId: 'rules2'},
        {ruleId: 1, rulesetId: 'rules1'}
      ];
      for (let i = 0; i < matchedRules.length; ++i) {
        chrome.test.assertEq(
            expectedMatches[i].ruleId, matchedRules[i].rule.ruleId);
        chrome.test.assertEq(
            expectedMatches[i].rulesetId, matchedRules[i].rule.rulesetId);
      }

      chrome.test.succeed();
    });
  },

  // Makes sure loads inside sandboxed iframes are considered thirdParty.
  function testSandboxedFrame() {
    resetMatchedRules();

    const baseUrl = getServerURL('a.com') +
        'extensions/api_test/declarative_net_request/on_rules_matched_debug/';
    const url = baseUrl + 'sandbox_container.html';
    const innerFrameUrl = baseUrl + 'blocked_frame.html';

    // Initiator inside sandbox is opaque.
    const kOpaqueInitiator = 'null';

    navigateTab(url, url, (tab) => {
      const expectedRuleInfo = {
        request: {
          documentLifecycle: 'active',
          frameType: 'sub_frame',
          initiator: kOpaqueInitiator,
          method: 'GET',
          type: 'sub_frame',
          tabId: tab.id,
          url: innerFrameUrl
        },
        rule: {ruleId: 6, rulesetId: 'rules2'}
      };
      verifyExpectedRuleInfo(expectedRuleInfo);
      chrome.test.succeed();
    });
  }
];

chrome.test.getConfig(function(config) {
  testServerPort = config.testServer.port;
  chrome.test.runTests(tests);
});
