// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function updateDynamicRules(options, expectedError) {
  return new Promise(resolve => {
    chrome.declarativeNetRequest.updateDynamicRules(options, () => {
      if (expectedError)
        chrome.test.assertLastError(expectedError);
      else
        chrome.test.assertNoLastError();

      resolve();
    });
  });
}

function createRuleWithID(id) {
  return {
    id,
    priority: 1,
    condition: {urlFilter: id.toString()},
    action: {type: 'block'},
  };
}

function createRegexRuleWithID(id) {
  return {
    id,
    priority: 1,
    condition: {regexFilter: id.toString()},
    action: {type: 'block'},
  };
}

function createLargeRegexRuleWithID(id) {
  let rule = createRegexRuleWithID(id);
  rule.condition.regexFilter = '.{512}x';
  return rule;
}

// Verifies the current set of rules. Ensures no error is signalled and proceeds
// to the next test.
function dynamicRulesEqual(expectedRules, ruleFilter) {
  chrome.test.assertNoLastError();

  return new Promise(resolve => {
    chrome.declarativeNetRequest.getDynamicRules(ruleFilter, actualRules => {
      chrome.test.assertNoLastError();

      // Sort by ID first since assertEq respects order of arrays.
      let comparator = (rule1, rule2) => rule1.id - rule2.id;
      actualRules.sort(comparator);
      expectedRules.sort(comparator);

      chrome.test.assertEq(expectedRules, actualRules);

      resolve();
    });
  });
};

let currentRules = [];

chrome.test.runTests([
  async function getRulesEmpty() {
    await dynamicRulesEqual([]);

    chrome.test.succeed();
  },

  async function addRulesEmpty() {
    await updateDynamicRules({});
    await dynamicRulesEqual([]);

    chrome.test.succeed();
  },

  async function addRules() {
    currentRules =
        [createRuleWithID(1), createRuleWithID(2), createRuleWithID(3)];
    await updateDynamicRules({addRules: currentRules});
    await dynamicRulesEqual(currentRules);

    chrome.test.succeed();
  },

  async function ruleIdsFilter() {
    await dynamicRulesEqual(
        currentRules.filter(({id}) => id !== 3), {ruleIds: [1, 2]});

    chrome.test.succeed();
  },

  async function removeRules() {
    // Remove rule with id 1, 2.
    // Also ensure rule ids which are not present are ignored.
    currentRules = currentRules.filter(({id}) => id === 3);
    await updateDynamicRules({addRules: [], removeRuleIds: [4, 5, 2, 1]});
    await dynamicRulesEqual(currentRules);

    chrome.test.succeed();
  },

  // Ensure we fail on adding a rule with a duplicate ID.
  async function duplicateID() {
    await updateDynamicRules(
        {addRules: [createRegexRuleWithID(3)]},
        'Rule with id 3 does not have a unique ID.');

    chrome.test.succeed();
  },

  // Ensure we get an error on adding a rule which exceeds the regex memory
  // limit.
  async function largeRegexError() {
    await updateDynamicRules(
        {addRules: [createLargeRegexRuleWithID(5)]},
        'Rule with id 5 was skipped as the "regexFilter" value exceeded the ' +
        '2KB memory limit when compiled. Learn more: ' +
        'https://developer.chrome.com/docs/extensions/reference/api/declarativeNetRequest#regex-rules');

    chrome.test.succeed();
  },
]);
