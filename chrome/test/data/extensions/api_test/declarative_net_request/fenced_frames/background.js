// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var expectedCallback;
var tab;

// Navigates to |url| and invokes |callback| when a rule has been queued.
function navigateTab(url, callback) {
  expectedCallback = callback;
  chrome.tabs.update(tab.id, {url: url});
}

var matchedRules = [];
var onRuleMatchedDebugCallback = (rule) => {
  matchedRules.push(rule);
  expectedCallback(tab);
};

var testServerPort;
var mparchEnabled;
function getServerURL(host) {
  if (!testServerPort)
    throw new Error('Called getServerURL outside of runTests.');
  return `http://${host}:${testServerPort}`;
}

function resetMatchedRules() {
  matchedRules = [];
}

function verifyExpectedRuleInfo(expectedRuleInfo) {
  const matchedRule = matchedRules[0];

  // The request ID may not be known but should be populated.
  chrome.test.assertTrue(matchedRule.request.hasOwnProperty('requestId'));
  delete matchedRule.request.requestId;

  chrome.test.assertEq(expectedRuleInfo, matchedRule);
}

var tests = [
  function setup() {
    chrome.declarativeNetRequest.onRuleMatchedDebug.addListener(
        onRuleMatchedDebugCallback);

    // Wait for a round trip to ensure the listener is properly added in the
    // browser process before initiating any requests.
    chrome.test.waitForRoundTrip('msg', chrome.test.succeed);
  },

  // Makes sure block rules apply for fenced frames.
  function testBlockRule() {
    resetMatchedRules();

    const baseUrl = getServerURL('a.com') +
          '/extensions/api_test/declarative_net_request/fenced_frames/';
    const url = baseUrl + 'blocked.html';
    const fencedFrameUrl = baseUrl + 'blocked_fenced_frame.html';
    navigateTab(url, (tab) => {
      const expectedRuleInfo = {
        request: {
          initiator: getServerURL('a.com'),
          method: 'GET',
          frameId: 4,
          parentFrameId: 0,
          type: 'sub_frame',
          tabId: tab.id,
          url: fencedFrameUrl
        },
        rule: {ruleId: 1, rulesetId: 'rules'}
      };
      verifyExpectedRuleInfo(expectedRuleInfo);
      chrome.test.succeed();
    });
  },

  // Makes sure rule 4 for subframes applies and not rule 2 for main frames
  // or rule 3 for thirdParty domains.
  function testAllowRule() {
    resetMatchedRules();

    const baseUrl = getServerURL('a.com') +
          '/extensions/api_test/declarative_net_request/fenced_frames/';
    const url = baseUrl + 'allow.html';
    const fencedFrameUrl = baseUrl + 'allowed_fenced_frame.html';
    navigateTab(url, (tab) => {
      const expectedRuleInfo = {
        request: {
          initiator: getServerURL('a.com'),
          method: 'GET',
          frameId: mparchEnabled ? 6 : 5,
          parentFrameId: 0,
          type: 'sub_frame',
          tabId: tab.id,
          url: fencedFrameUrl
        },
        rule: {ruleId: 4, rulesetId: 'rules'}
      };
      verifyExpectedRuleInfo(expectedRuleInfo);
      chrome.test.succeed();
    });
  }
];

chrome.test.getConfig(async (config) => {
  testServerPort = config.testServer.port;
  mparchEnabled = config.customArg == 'MPArch';
  tab = await new Promise(function(resolve, reject) {
    chrome.tabs.create({"url": "about:blank"}, (value) => {
      resolve(value);
    });
  });

  chrome.test.runTests(tests);
});
