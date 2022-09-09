// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var updateDynamicRules = chrome.declarativeNetRequest.updateDynamicRules;
var getDynamicRules = chrome.declarativeNetRequest.getDynamicRules;
var ruleLimit = chrome.declarativeNetRequest.MAX_NUMBER_OF_DYNAMIC_AND_SESSION_RULES;
var regexRuleLimit = chrome.declarativeNetRequest.MAX_NUMBER_OF_REGEX_RULES;
var nextId = 1;

var createRuleWithID = function(id) {
  return {
    id: id,
    priority: 1,
    condition: {urlFilter: id.toString()},
    action: {type: 'block'},
  };
};

var createRegexRuleWithID = function(id) {
  return {
    id: id,
    priority: 1,
    condition: {regexFilter: id.toString()},
    action: {type: 'block'},
  };
};

// Verifies the current set of rules. Ensures no error is signalled and proceeds
// to the next test.
var verifyCurrentRulesCallback = function() {
  chrome.test.assertNoLastError();

  getDynamicRules(function(rules) {
    chrome.test.assertNoLastError();

    var comparator = function(rule1, rule2) {
      return rule1.id - rule2.id;
    };

    // Sort by ID first since assertEq respects order of arrays.
    rules.sort(comparator)
    currentRules.sort(comparator);
    chrome.test.assertEq(currentRules, rules);

    chrome.test.succeed();
  });
};
var currentRules = [];

chrome.test.runTests([
  // Ensure that an extension can add up to |regexRuleLimit| number of regex
  // rules.
  function regexRuleLimitReached() {
    var numRulesToAdd = regexRuleLimit;
    var newRules = [];
    while (newRules.length < regexRuleLimit)
      newRules.push(createRegexRuleWithID(nextId++));

    currentRules = newRules.concat(currentRules);
    chrome.test.assertEq(regexRuleLimit, currentRules.length);
    updateDynamicRules({addRules: newRules}, verifyCurrentRulesCallback);
  },

  // Ensure that adding more regex rules than |regexRuleLimit| causes an error.
  function regexRuleLimitError() {
    updateDynamicRules(
        {addRules: [createRegexRuleWithID(nextId++)]},
        chrome.test.callbackFail(
            'Dynamic rule count for regex rules exceeded.'));
  },

  // Ensure we can add up to |ruleLimit| no. of rules.
  function ruleLimitReached() {
    var numRulesToAdd = ruleLimit - currentRules.length;
    var newRules = [];
    while (newRules.length < numRulesToAdd)
      newRules.push(createRuleWithID(nextId++));

    currentRules = newRules.concat(currentRules);
    chrome.test.assertEq(ruleLimit, currentRules.length);
    updateDynamicRules(
        {addRules: newRules, removeRuleIds: []}, verifyCurrentRulesCallback);
  },

  // Ensure we can't add more than |ruleLimit| rules.
  function ruleLimitError() {
    updateDynamicRules(
        {addRules: [createRuleWithID(nextId++)]},
        chrome.test.callbackFail('Dynamic rule count exceeded.'));

}

]);
