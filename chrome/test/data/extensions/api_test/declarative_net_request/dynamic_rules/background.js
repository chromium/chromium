// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var updateDynamicRules = chrome.declarativeNetRequest.updateDynamicRules;
var getDynamicRules = chrome.declarativeNetRequest.getDynamicRules;
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

var createLargeRegexRuleWithID = function(id) {
  var rule = createRegexRuleWithID(id);
  rule.condition.regexFilter = '.{512}x';
  return rule;
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
    updateDynamicRules({}, verifyCurrentRulesCallback);
  },

  function addRules() {
    currentRules =
        [createRuleWithID(1), createRuleWithID(2), createRuleWithID(3)];
    updateDynamicRules({addRules: currentRules}, verifyCurrentRulesCallback);
    nextId = 4;
  },

  function removeRules() {
    // Remove rule with id 1, 2.
    // Also ensure rule ids which are not present are ignored.
    currentRules = currentRules.filter(rule => rule.id === 3);
    updateDynamicRules(
        {addRules: [], removeRuleIds: [4, 5, 2, 1]},
        verifyCurrentRulesCallback);
  },

  // Ensure we fail on adding a rule with a duplicate ID.
  function duplicateID() {
    updateDynamicRules(
        {addRules: [createRegexRuleWithID(3)]},
        chrome.test.callbackFail('Rule with id 3 does not have a unique ID.'));
  },

  // Ensure we get an error on adding a rule which exceeds the regex memory
  // limit.
  function largeRegexError() {
    updateDynamicRules(
        {addRules: [createLargeRegexRuleWithID(5)]},
        chrome.test.callbackFail(
            'Rule with id 5 specified a more complex regex than allowed as ' +
            'part of the "regexFilter" key.'));
  },
]);
