// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var updateDynamicRules = chrome.declarativeNetRequest.updateDynamicRules;
var getDynamicRules = chrome.declarativeNetRequest.getDynamicRules;
var ruleLimit = chrome.declarativeNetRequest.MAX_NUMBER_OF_DYNAMIC_RULES;

var createRuleWithID = function(id) {
  return {
    id: id,
    condition: {urlFilter: id.toString()},
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
  function getRulesEmpty() {
    verifyCurrentRulesCallback();
  },

  function addRulesEmpty() {
    updateDynamicRules([], currentRules, verifyCurrentRulesCallback);
  },

  function addRules() {
    currentRules =
        [createRuleWithID(1), createRuleWithID(2), createRuleWithID(3)];
    updateDynamicRules([], currentRules, verifyCurrentRulesCallback);
  },

  function removeRules() {
    // Remove rule with id 1, 2.
    // Also ensure rule ids which are not present are ignored.
    currentRules = currentRules.filter(rule => rule.id === 3);
    updateDynamicRules([4, 5, 2, 1], [], verifyCurrentRulesCallback);
  },

  // Ensure we fail on adding a rule with a duplicate ID.
  function duplicateID() {
    updateDynamicRules(
        [], currentRules,
        chrome.test.callbackFail('Rule with id 3 does not have a unique ID.'));
  },

  // Ensure we can add up to |ruleLimit| no. of rules.
  function ruleLimitReached() {
    // There is already a single rule present with 'id' 3.
    var numRulesToAdd = ruleLimit - 1;
    var newRules = [];
    for (var id = 4; newRules.length < numRulesToAdd; ++id)
      newRules.push(createRuleWithID(id));

    currentRules = newRules.concat(currentRules);
    chrome.test.assertEq(ruleLimit, currentRules.length);
    updateDynamicRules([], newRules, verifyCurrentRulesCallback);
  },

  // Ensure we can't add more than |ruleLimit| rules.
  function ruleLimitError() {
    updateDynamicRules(
        [], [createRuleWithID(1)],
        chrome.test.callbackFail('Dynamic rule count exceeded.'));
  }
]);
