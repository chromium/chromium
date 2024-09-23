// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testMatchOutcome(options) {
  return new Promise(resolve => {
    chrome.declarativeNetRequest.testMatchOutcome(options, resolve);
  });
}

function updateSessionRules(options) {
  return new Promise(resolve => {
    chrome.declarativeNetRequest.updateSessionRules(options, resolve);
  });
}

chrome.test.runTests([

  function setup() {
    // Enable the extension's rulesets here, instead of by default in the
    // manifest, to ensure that the rulesets are ready before running the tests
    // and avoid race condition. This works since the tests run sequentially.
    chrome.declarativeNetRequest.updateEnabledRulesets(
        {enableRulesetIds: ['rules1', 'rules2', 'modifyheaders']},
        chrome.test.succeed);
  },

  function testInvalidUrl() {
    chrome.declarativeNetRequest.testMatchOutcome(
        {url: 'http:://example.example', type: 'sub_frame'},
        chrome.test.callbackFail('Invalid test request URL.'));
  },

  function testInvalidInitiator() {
    chrome.declarativeNetRequest.testMatchOutcome(
        {
          url: 'http://example.example',
          type: 'sub_frame',
          initiator: 'http:://example.example'
        },
        chrome.test.callbackFail('Invalid test request initiator.'));
  },

  function testInvalidTabID() {
    chrome.declarativeNetRequest.testMatchOutcome(
        {url: 'http://example.example', type: 'sub_frame', tabId: -2},
        chrome.test.callbackFail('Invalid test request tab ID.'));
  },

  async function testNoMatch() {
    let result = await testMatchOutcome({
      url: 'https://no-match.example/path',
      type: 'main_frame',
      method: 'get'
    });
    chrome.test.assertEq({matchedRules: []}, result);

    chrome.test.succeed();
  },

  async function testRequestMatch() {
    let result = await testMatchOutcome(
        {url: 'https://block.example/path', type: 'main_frame', method: 'get'});
    chrome.test.assertEq({matchedRules: []}, result);

    result = await testMatchOutcome(
        {url: 'https://block.example/path', type: 'sub_frame', method: 'get'});
    chrome.test.assertEq(
        {matchedRules: [{ruleId: 1, rulesetId: 'rules2'}]}, result);

    result = await testMatchOutcome(
        {url: 'https://block.example/path', type: 'sub_frame'});
    chrome.test.assertEq(
        {matchedRules: [{ruleId: 1, rulesetId: 'rules2'}]}, result);

    result = await testMatchOutcome(
        {url: 'https://block2.example/path', type: 'sub_frame', method: 'get'});
    chrome.test.assertEq({matchedRules: []}, result);

    result = await testMatchOutcome(
        {url: 'https://block2.example/path', type: 'sub_frame'});
    chrome.test.assertEq({matchedRules: []}, result);

    result = await testMatchOutcome({
      url: 'https://block2.example/path',
      type: 'sub_frame',
      method: 'post'
    });
    chrome.test.assertEq(
        {matchedRules: [{ruleId: 4, rulesetId: 'rules1'}]}, result);

    result = await testMatchOutcome(
        {url: 'ws://block.example/path', type: 'websocket'});
    chrome.test.assertEq(
        {matchedRules: [{ruleId: 1, rulesetId: 'rules2'}]}, result);

    result = await testMatchOutcome({
      url: 'https://allow-all-requests.example/path',
      type: 'main_frame',
      method: 'get'
    });
    chrome.test.assertEq({matchedRules: []}, result);

    result = await testMatchOutcome({
      url: 'https://allow-all-requests.example/path',
      type: 'main_frame',
      method: 'post'
    });
    chrome.test.assertEq(
        {matchedRules: [{ruleId: 1, rulesetId: 'rules1'}]}, result);

    // HTTP request method is RequestMethod_NON_HTTP since this isn't a HTTP
    // URL and therefore the request won't match.
    result = await testMatchOutcome({
      url: 'ws://allow-all-requests.example/path',
      type: 'main_frame',
      method: 'post'
    });
    chrome.test.assertEq({matchedRules: []}, result);

    chrome.test.succeed();
  },

  async function testInitiatorMatch() {
    let result = await testMatchOutcome(
        {url: 'https://block3.example/path', type: 'sub_frame', method: 'get'});
    chrome.test.assertEq({matchedRules: []}, result);

    result = await testMatchOutcome({
      url: 'https://block3.example/path',
      initiator: 'https://wrong-initiator.example/path',
      type: 'sub_frame',
      method: 'get'
    });
    chrome.test.assertEq({matchedRules: []}, result);

    result = await testMatchOutcome({
      url: 'https://block3.example/path',
      initiator: 'https://initiator.example/path',
      type: 'sub_frame',
      method: 'get'
    });
    chrome.test.assertEq(
        {matchedRules: [{ruleId: 5, rulesetId: 'rules1'}]}, result);

    result = await testMatchOutcome({
      url: 'https://block4.example/path',
      initiator: 'https://block4.example/path',
      type: 'sub_frame',
      method: 'get'
    });
    chrome.test.assertEq({matchedRules: []}, result);

    result = await testMatchOutcome({
      url: 'https://block4.example/path',
      initiator: 'https://different.example/path',
      type: 'sub_frame',
      method: 'get'
    });
    chrome.test.assertEq(
        {matchedRules: [{ruleId: 6, rulesetId: 'rules1'}]}, result);

    chrome.test.succeed();
  },

  async function testModifyHeadersMatch() {
    let result = await testMatchOutcome({
      url: 'https://modify-headers.example/path',
      type: 'sub_frame',
      method: 'get'
    });
    chrome.test.assertEq(
        {matchedRules: [{ruleId: 1, rulesetId: 'modifyheaders'}]}, result);

    result = await testMatchOutcome({
      url: 'https://modify-headers.example/path',
      type: 'main_frame',
      method: 'get'
    });
    chrome.test.assertEq(
        {
          matchedRules: [
            {ruleId: 1, rulesetId: 'modifyheaders'},
            {ruleId: 2, rulesetId: 'modifyheaders'}
          ]
        },
        result);

    // Only rule 1 should match since:
    // - rule 2 is outprioritized by rule 3 (allow)
    // - rule 3 does not match since there rule 1 (modifyHeaders) matches and
    //   has a higher priority.
    result = await testMatchOutcome({
      url: 'https://modify-headers.example2/path',
      type: 'main_frame',
      method: 'get'
    });
    chrome.test.assertEq(
        {matchedRules: [{ruleId: 1, rulesetId: 'modifyheaders'}]}, result);

    // Only rule 4 should match since it outprioritizes both rules 1 and 2.
    result = await testMatchOutcome({
      url: 'https://modify-headers.example3/path',
      type: 'main_frame',
      method: 'get'
    });
    chrome.test.assertEq(
        {matchedRules: [{ruleId: 4, rulesetId: 'modifyheaders'}]}, result);

    chrome.test.succeed();
  },

  async function testTabIDMatch() {
    let result = await testMatchOutcome(
        {url: 'https://tabid.example/path', type: 'image', method: 'get'});
    chrome.test.assertEq({matchedRules: []}, result);
    result = await testMatchOutcome({
      url: 'https://tabid.example/path',
      type: 'image',
      method: 'get',
      tabId: 31337
    });
    chrome.test.assertEq({matchedRules: []}, result);

    await updateSessionRules({
      addRules: [{
        id: 1337,
        priority: 1,
        action: {type: 'block'},
        condition: {requestDomains: ['tabid.example'], tabIds: [31337]}
      }]
    });

    result = await testMatchOutcome(
        {url: 'https://tabid.example/path', type: 'image', method: 'get'});
    chrome.test.assertEq({matchedRules: []}, result);

    result = await testMatchOutcome({
      url: 'https://tabid.example/path',
      type: 'image',
      method: 'get',
      tabId: 123
    });
    chrome.test.assertEq({matchedRules: []}, result);

    result = await testMatchOutcome({
      url: 'https://tabid.example/path',
      type: 'image',
      method: 'get',
      tabId: 31337
    });
    chrome.test.assertEq(
        {matchedRules: [{ruleId: 1337, rulesetId: '_session'}]}, result);

    chrome.test.succeed();
  },

  async function testRedirectMatch() {
    // Redirect rule with host permissions should apply.
    let result = await testMatchOutcome({
      url: 'https://allowed-redirect.example/ad.js',
      initiator: 'https://allowed-redirect.example',
      type: 'script'
    });
    chrome.test.assertEq(
        {matchedRules: [{ruleId: 2, rulesetId: 'rules2'}]}, result);
    result = await testMatchOutcome(
        {url: 'https://allowed-redirect.example/ad.js', type: 'script'});
    chrome.test.assertEq(
        {matchedRules: [{ruleId: 2, rulesetId: 'rules2'}]}, result);

    // No host permission for request URL.
    result = await testMatchOutcome(
        {url: 'https://not-allowed-redirect.example/ad1.js', type: 'script'});
    chrome.test.assertEq({matchedRules: []}, result);

    // No host permission for initiator URL.
    result = await testMatchOutcome({
      url: 'https://allowed-redirect.example/ad.js',
      initiator: 'https://not-allowed-redirect.example',
      type: 'script'
    });
    chrome.test.assertEq({matchedRules: []}, result);

    chrome.test.succeed();
  }
]);
