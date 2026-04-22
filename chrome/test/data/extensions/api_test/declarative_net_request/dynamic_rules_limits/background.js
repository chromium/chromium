// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const updateDynamicRules = chrome.declarativeNetRequest.updateDynamicRules;
const getDynamicRules = chrome.declarativeNetRequest.getDynamicRules;

// Rule limits are actually set after the browser replies to the extension's
// "ready" message.
let ruleLimit = -1;
let unsafeRuleLimit = -1;
let regexRuleLimit = -1;

let nextId = 1;

const createRuleWithID = function(id) {
  return {
    id: id,
    priority: 1,
    condition: {urlFilter: id.toString()},
    action: {type: 'block'},
  };
};

const createRedirectRuleWithID = function(id) {
  return {
    id: id,
    priority: 1,
    condition: {urlFilter: id.toString()},
    action: {type: 'redirect', redirect: {url: 'https://example.com'}},
  };
};

const createRegexRuleWithID = function(id) {
  return {
    id: id,
    priority: 1,
    condition: {regexFilter: id.toString()},
    action: {type: 'block'},
  };
};

// Verifies the current set of rules. Ensures no error is signalled and proceeds
// to the next test.
const verifyCurrentRulesCallback = function() {
  chrome.test.assertNoLastError();

  getDynamicRules(function(rules) {
    chrome.test.assertNoLastError();

    const comparator = function(rule1, rule2) {
      return rule1.id - rule2.id;
    };

    // Sort by ID first since assertEq respects order of arrays.
    rules.sort(comparator);
    currentRules.sort(comparator);
    chrome.test.assertEq(currentRules, rules);

    chrome.test.succeed();
  });
};
let currentRules = [];

const testCases = [
  // Sanity check that rule limits received from the browser have been set.
  function checkRuleLimits() {
    chrome.test.assertTrue(ruleLimit > 0);
    chrome.test.assertTrue(unsafeRuleLimit > 0);
    chrome.test.assertTrue(regexRuleLimit > 0);
    chrome.test.succeed();
  },

  // Ensure that an extension can add up to `regexRuleLimit` number of regex
  // rules.
  function regexRuleLimitReached() {
    const newRules = [];
    while (newRules.length < regexRuleLimit) {
      newRules.push(createRegexRuleWithID(nextId++));
    }

    currentRules = newRules.concat(currentRules);
    chrome.test.assertEq(regexRuleLimit, currentRules.length);
    updateDynamicRules({addRules: newRules}, verifyCurrentRulesCallback);
  },

  // Ensure that adding more regex rules than `regexRuleLimit` causes an error.
  function regexRuleLimitError() {
    updateDynamicRules(
        {addRules: [createRegexRuleWithID(nextId++)]},
        chrome.test.callbackFail(
            'Dynamic rule count for regex rules exceeded.'));
  },

  // Ensure that an extension can add up to `unsafeRuleLimit` number of "unsafe"
  // rules.
  function unsafeRuleLimitReached() {
    const newRules = [];
    while (newRules.length < unsafeRuleLimit) {
      newRules.push(createRedirectRuleWithID(nextId++));
    }

    currentRules = newRules.concat(currentRules);
    chrome.test.assertEq(regexRuleLimit + unsafeRuleLimit, currentRules.length);
    updateDynamicRules({addRules: newRules}, verifyCurrentRulesCallback);
  },

  // Ensure that adding more "unsafe" rules than `unsafeRuleLimit` causes an
  // error.
  function unsafeRuleLimitError() {
    updateDynamicRules(
        {addRules: [createRedirectRuleWithID(nextId++)]},
        chrome.test.callbackFail('Dynamic unsafe rule count exceeded.'));
  },

  // Ensure we can add up to `ruleLimit` no. of rules.
  function ruleLimitReached() {
    const numRulesToAdd = ruleLimit - currentRules.length;
    const newRules = [];
    while (newRules.length < numRulesToAdd) {
      newRules.push(createRuleWithID(nextId++));
    }

    currentRules = newRules.concat(currentRules);
    chrome.test.assertEq(ruleLimit, currentRules.length);
    updateDynamicRules(
        {addRules: newRules, removeRuleIds: []}, verifyCurrentRulesCallback);
  },

  // Ensure we can't add more than `ruleLimit` rules.
  function ruleLimitError() {
    updateDynamicRules(
        {addRules: [createRuleWithID(nextId++)]},
        chrome.test.callbackFail('Dynamic rule count exceeded.'));
  },
];

chrome.test.sendMessage('ready', ruleLimitsStr => {
  const ruleLimits = JSON.parse(ruleLimitsStr);
  ruleLimit = ruleLimits.ruleLimit;
  unsafeRuleLimit = ruleLimits.unsafeRuleLimit;
  regexRuleLimit = ruleLimits.regexRuleLimit;

  chrome.test.runTests(testCases);
});
