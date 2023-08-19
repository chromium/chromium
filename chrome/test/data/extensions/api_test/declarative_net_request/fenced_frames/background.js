// Copyright 2021 The Chromium Authors
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
var documentIds = [];
var nextDocumentId = 1;

var onRuleMatchedDebugCallback = (rule) => {
  matchedRules.push(rule);
  expectedCallback(tab);
};

var testServerPort;
function getServerURL(host) {
  if (!testServerPort)
    throw new Error('Called getServerURL outside of runTests.');
  return `https://${host}:${testServerPort}`;
}

function resetMatchedRules() {
  matchedRules = [];
}

function verifyExpectedRuleInfo(expectedRuleInfo) {
  const matchedRule = matchedRules[0];

  // The request ID may not be known but should be populated.
  chrome.test.assertTrue(matchedRule.request.hasOwnProperty('requestId'));
  delete matchedRule.request.requestId;

  // Since the documentId/parentDocumentId is a unique random identifier it is
  // not useful to tests. Normalize them so that test cases can assert
  // against a fixed number.
  if ('parentDocumentId' in matchedRule.request) {
    if (documentIds[matchedRule.request.parentDocumentId] === undefined) {
      documentIds[matchedRule.request.parentDocumentId] = nextDocumentId++;
    }
    matchedRule.request.parentDocumentId =
      documentIds[matchedRule.request.parentDocumentId];
  }
  if ('documentId' in matchedRule.request) {
    if (documentIds[matchedRule.request.documentId] === undefined) {
      documentIds[matchedRule.request.documentId] = nextDocumentId++;
    }
    matchedRule.request.documentId =
      documentIds[matchedRule.request.documentId];
  }

  chrome.test.assertEq(expectedRuleInfo, matchedRule);
}

// Opaque initiators serialize to "null".
const kOpaqueInitiator = "null";

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

    const baseUrl = getServerURL('a.test') +
          '/extensions/api_test/declarative_net_request/fenced_frames/';
    const url = baseUrl + 'blocked.html';
    const fencedFrameUrl = baseUrl + 'blocked_fenced_frame.html';
    navigateTab(url, (tab) => {
      const expectedRuleInfo = {
        request: {
          initiator: kOpaqueInitiator,
          method: 'GET',
          frameId: 5,
          documentLifecycle: 'active',
          frameType: 'fenced_frame',
          parentDocumentId: 1,
          parentFrameId: 0,
          type: 'sub_frame',
          tabId: tab.id,
          url: fencedFrameUrl
        },
        rule: {ruleId: 1, rulesetId: 'rules'}
      };
      verifyExpectedRuleInfo(expectedRuleInfo);

      const getFencedFrameWidth =
        '(async function() {' +
        '  while(true) { ' +
        '    await new Promise(requestAnimationFrame);' +
        '    let width =' +
        '      document.getElementsByTagName("fencedframe")[0].clientWidth;' +
        '    if (width == 0) {' +
        '      chrome.runtime.sendMessage({width: width});' +
        '      break;' +
        '    }' +
        '  }' +
        '})()';

      chrome.runtime.onMessage.addListener(results => {
        // Ensure the clientWidth is 0 indicating
        // the frame has no layout size and was
        // collapsed correctly.
        chrome.test.assertEq(0, results.width);
        chrome.test.succeed();
      });
      chrome.tabs.executeScript(tab.id, {frameId: 0,
                                         code: getFencedFrameWidth});
    });
  },

  // Makes sure rule 4 for subframes applies and not rule 2 for main frames
  // or rule 3 for thirdParty domains.
  function testAllowRule() {
    resetMatchedRules();

    const baseUrl = getServerURL('a.test') +
          '/extensions/api_test/declarative_net_request/fenced_frames/';
    const url = baseUrl + 'allow.html';
    const fencedFrameUrl = baseUrl + 'allowed_fenced_frame.html';
    navigateTab(url, (tab) => {
      const expectedRuleInfo = {
        request: {
          initiator: kOpaqueInitiator,
          method: 'GET',
          frameId: 7,
          documentLifecycle: 'active',
          frameType: 'fenced_frame',
          parentDocumentId: 2,
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
  },

  // Uses rule 5 to allow a subresource inside the fenced frame.
  function testAllowResourceRule() {
    resetMatchedRules();

    const baseUrl = getServerURL('a.test') +
      '/extensions/api_test/declarative_net_request/fenced_frames/';
    const url = baseUrl + 'resource1.html';
    const matchedImageUrl = baseUrl + 'icon1.png';
    navigateTab(url, (tab) => {
      const expectedRuleInfo = {
        request: {
          initiator: getServerURL('a.test'),
          method: 'GET',
          documentId: 4,
          frameId: 9,
          documentLifecycle: 'active',
          frameType: 'fenced_frame',
          parentDocumentId: 3,
          parentFrameId: 0,
          type: 'image',
          tabId: tab.id,
          url: matchedImageUrl
        },
        rule: { ruleId: 5, rulesetId: 'rules' }
      };
      verifyExpectedRuleInfo(expectedRuleInfo);
      chrome.test.succeed();
    });
  },

  // Uses rule 6 to block a subresource inside the fenced frame.
  function testBlockResourceRule() {
    resetMatchedRules();

    const baseUrl = getServerURL('a.test') +
      '/extensions/api_test/declarative_net_request/fenced_frames/';
    const url = baseUrl + 'resource2.html';
    const matchedImageUrl = baseUrl + 'icon2.png';
    navigateTab(url, (tab) => {
      const expectedRuleInfo = {
        request: {
          initiator: getServerURL('a.test'),
          method: 'GET',
          documentId: 6,
          frameId: 11,
          documentLifecycle: 'active',
          frameType: 'fenced_frame',
          parentDocumentId: 5,
          parentFrameId: 0,
          type: 'image',
          tabId: tab.id,
          url: matchedImageUrl
        },
        rule: { ruleId: 6, rulesetId: 'rules' }
      };
      verifyExpectedRuleInfo(expectedRuleInfo);
      chrome.test.succeed();
    });
  }
];

chrome.test.getConfig(async (config) => {
  testServerPort = config.testServer.port;
  tab = await new Promise(function(resolve, reject) {
    chrome.tabs.create({"url": "about:blank"}, (value) => {
      resolve(value);
    });
  });

  chrome.test.runTests(tests);
});
