// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testMatchOutcome(options) {
  return new Promise(resolve => {
    chrome.declarativeNetRequest.testMatchOutcome(options, resolve);
  });
}

chrome.test.runTests([
  function setup() {
    // Enable the extension's rulesets here, instead of by default in the
    // manifest, to ensure that the rulesets are ready before running the tests
    // and avoid race conditions. This works since the tests run sequentially.
    chrome.declarativeNetRequest.updateEnabledRulesets(
        {
          enableRulesetIds: ['header_matching', 'inter_phase', 'modify_headers']
        },
        chrome.test.succeed);
  },

  // Test that the provided response headers match the right rules.
  async function testHeadersMatch() {
    let result = await testMatchOutcome({
      url: 'https://header-matching.example-1/path',
      type: 'main_frame',
      method: 'get',
      responseHeaders: {'example-header': ['value']}
    });
    chrome.test.assertEq(
        {matchedRules: [{ruleId: 1, rulesetId: 'header_matching'}]}, result);

    result = await testMatchOutcome({
      url: 'https://header-matching.example-1/path',
      type: 'main_frame',
      method: 'get',
      responseHeaders: {}
    });
    chrome.test.assertEq({matchedRules: []}, result);

    result = await testMatchOutcome({
      url: 'https://excluded-header-matching.example/path',
      type: 'main_frame',
      method: 'get'
    });
    chrome.test.assertEq(
        {matchedRules: [{ruleId: 2, rulesetId: 'header_matching'}]}, result);

    result = await testMatchOutcome({
      url: 'https://excluded-header-matching.example/path',
      type: 'main_frame',
      method: 'get',
      responseHeaders: {'nonsense-header': ['value']}
    });
    chrome.test.assertEq({matchedRules: []}, result);

    chrome.test.succeed();
  },

  // Test malformed response header arguments, invalid header names and values.
  function testMalformedHeadersInTestRequest() {
    chrome.declarativeNetRequest.testMatchOutcome(
        {
          url: 'https://header-matching.example-1/path',
          type: 'main_frame',
          method: 'get',
          responseHeaders: {'@invalid-header': ['value']}
        },
        chrome.test.callbackFail('Invalid header name "@invalid-header".'));

    chrome.declarativeNetRequest.testMatchOutcome(
        {
          url: 'https://header-matching.example-1/path',
          type: 'main_frame',
          method: 'get',
          responseHeaders: {'header': 'not-list'}
        },
        chrome.test.callbackFail(
            'Values for header "header" must be specified as a list.'));

    chrome.declarativeNetRequest.testMatchOutcome(
        {
          url: 'https://header-matching.example-1/path',
          type: 'main_frame',
          method: 'get',
          responseHeaders: {'header': ['invalid\nvalue']}
        },
        chrome.test.callbackFail('Invalid header value for header "header".'));

    chrome.test.succeed();
  },

  // Test inter-phase interactions.
  async function testRulesFromDifferentRequestPhases() {
    // A request blocked before it is sent shouldn't be matched with rules that
    // match on response headers.
    let result = await testMatchOutcome(
        {url: 'https://b.com/path', type: 'main_frame', method: 'get'});
    chrome.test.assertEq(
        {matchedRules: [{ruleId: 11, rulesetId: 'inter_phase'}]}, result);

    // Test rule 10 vs an allow rule of lesser priority (rule 12).
    result = await testMatchOutcome(
        {url: 'https://c.com/path', type: 'main_frame', method: 'get'});
    chrome.test.assertEq(
        {matchedRules: [{ruleId: 10, rulesetId: 'inter_phase'}]}, result);

    // Test rule 10 vs an allow rule of greater priority (rule 13).
    result = await testMatchOutcome(
        {url: 'https://d.com/path', type: 'main_frame', method: 'get'});
    chrome.test.assertEq(
        {matchedRules: [{ruleId: 13, rulesetId: 'inter_phase'}]}, result);

    chrome.test.succeed();
  },

  // Test inter-phase interactions with modify header rules.
  async function testModifyHeaderRulesFromDifferentRequestPhases() {
    // Request is blocked based on its headers, so only the block rule should
    // match.
    let result = await testMatchOutcome(
        {url: 'https://block-mh.com/path', type: 'main_frame', method: 'get'});
    chrome.test.assertEq(
        {matchedRules: [{ruleId: 103, rulesetId: 'modify_headers'}]}, result);

    // Allow rule 101 outprioritizes modifyHeaders rule 106, but not rule 107.
    result = await testMatchOutcome(
        {url: 'https://br-allow.com/path', type: 'main_frame', method: 'get'});
    chrome.test.assertEq(
        {matchedRules: [{ruleId: 107, rulesetId: 'modify_headers'}]}, result);

    // Allow rule 102 outprioritizes all matching modifyHeaders rules.
    result = await testMatchOutcome(
        {url: 'https://hr-allow.com/path', type: 'main_frame', method: 'get'});
    chrome.test.assertEq(
        {matchedRules: [{ruleId: 102, rulesetId: 'modify_headers'}]}, result);

    // Matched modifyHeaders rules should be sorted in descending priority
    // order.
    result = await testMatchOutcome(
        {url: 'https://merge-mh.com/path', type: 'main_frame', method: 'get'});
    chrome.test.assertEq(
        {
          matchedRules: [
            {ruleId: 107, rulesetId: 'modify_headers'},
            {ruleId: 105, rulesetId: 'modify_headers'},
            {ruleId: 106, rulesetId: 'modify_headers'},
            {ruleId: 104, rulesetId: 'modify_headers'}
          ]
        },
        result);

    chrome.test.succeed();
  }
]);
