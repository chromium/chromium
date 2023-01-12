// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

async function updateStaticRules(options, expectedError) {
  if (expectedError) {
    await chrome.test.assertPromiseRejects(
        chrome.declarativeNetRequest.updateStaticRules(options),
        expectedError);
  } else {
    await chrome.declarativeNetRequest.updateStaticRules(options);
  }
}

async function getActiveRules() {
  let activeRules = [];
  for (const rulesetId of ['rules1', 'rules2']) {
    for (const ruleId of [1, 2, 3]) {
      let result = await chrome.declarativeNetRequest.testMatchOutcome(
          {url: `https://${rulesetId}-${ruleId}.example.com/`,
           type: 'main_frame', method: 'get'});
      if (result.matchedRules.length > 0) {
        chrome.test.assertEq(
            {matchedRules: [{ruleId: ruleId, rulesetId: rulesetId}]},
             result);
        activeRules.push(`${rulesetId}-${ruleId}`);
      } else {
        chrome.test.assertEq({matchedRules: []}, result);
      }
    }
  }
  return activeRules;
}

async function getDisabledRuleIds(rulesetId) {
  return await chrome.declarativeNetRequest.getDisabledRuleIds(
      {rulesetId: rulesetId});
}

async function verifyGetDisabledRuleIdsError(rulesetId, expectedError) {
  await chrome.test.assertPromiseRejects(
      chrome.declarativeNetRequest.getDisabledRuleIds({rulesetId: rulesetId}),
      expectedError);
}

chrome.test.runTests([
  async function enableRulesets() {
    // Enable the extension's rulesets here, instead of by default in the
    // manifest, to ensure that the rulesets are ready before running the tests
    // and avoid race condition. This works since the tests run sequentially.
    await chrome.declarativeNetRequest.updateEnabledRulesets(
        {enableRulesetIds: ['rules1', 'rules2']});

    chrome.test.assertEq(['rules1-1', 'rules1-2', 'rules1-3', 'rules2-1',
                          'rules2-2', 'rules2-3'],
                         await getActiveRules());
    chrome.test.assertEq([], await getDisabledRuleIds("rules1"));
    chrome.test.assertEq([], await getDisabledRuleIds("rules2"));
    chrome.test.succeed();
  },
  async function disableRuleset1Rules() {
    // Disable rules1-2 and rules1-3.
    await updateStaticRules({rulesetId: 'rules1', disableRuleIds: [2, 3]});

    chrome.test.assertEq(['rules1-1', 'rules2-1', 'rules2-2', 'rules2-3'],
                         await getActiveRules());
    chrome.test.assertEq([2, 3], await getDisabledRuleIds("rules1"));
    chrome.test.assertEq([], await getDisabledRuleIds("rules2"));
    chrome.test.succeed();
  },
  async function disableRulesets() {
    // Disable all rulesets (rules1, rules2). All rules in the disabled
    // rulesets must be inactive.
    await chrome.declarativeNetRequest.updateEnabledRulesets(
        {disableRulesetIds: ['rules1', 'rules2']});

    chrome.test.assertEq([], await getActiveRules());
    chrome.test.assertEq([2, 3], await getDisabledRuleIds("rules1"));
    chrome.test.assertEq([], await getDisabledRuleIds("rules2"));
    chrome.test.succeed();
  },
  async function enableRuleset1Rule() {
    // Enable ruleset1-3. Still all rules are inactive, but rules1-3 is
    // enabled internally.
    await updateStaticRules({rulesetId: 'rules1', enableRuleIds: [3]});

    chrome.test.assertEq([], await getActiveRules());
    chrome.test.assertEq([2], await getDisabledRuleIds("rules1"));
    chrome.test.assertEq([], await getDisabledRuleIds("rules2"));
    chrome.test.succeed();
  },
  async function disableRuleset2Rules() {
    // Disable rules2-2 and rules2-3. Still all rules are inactive, but
    // rules2-2 and rules2-3 are disabled internally.
    await updateStaticRules({rulesetId: 'rules2', disableRuleIds: [2, 3]});

    chrome.test.assertEq([], await getActiveRules());
    chrome.test.assertEq([2], await getDisabledRuleIds("rules1"));
    chrome.test.assertEq([2, 3], await getDisabledRuleIds("rules2"));
    chrome.test.succeed();
  },
  async function enableRulesetsAgain() {
    // Enable all rulesets again to check whether the extension keeps the
    // disabled static rules information.
    await chrome.declarativeNetRequest.updateEnabledRulesets(
        {enableRulesetIds: ['rules1', 'rules2']});

    chrome.test.assertEq(['rules1-1', 'rules1-3', 'rules2-1'],
                         await getActiveRules());
    chrome.test.assertEq([2], await getDisabledRuleIds("rules1"));
    chrome.test.assertEq([2, 3], await getDisabledRuleIds("rules2"));
    chrome.test.succeed();
  },
  async function enableRuleset2Rule() {
    // Enable rules2-2.
    await updateStaticRules({rulesetId: 'rules2', enableRuleIds: [2]});

    chrome.test.assertEq(['rules1-1', 'rules1-3', 'rules2-1', 'rules2-2'],
                         await getActiveRules());
    chrome.test.assertEq([2], await getDisabledRuleIds("rules1"));
    chrome.test.assertEq([3], await getDisabledRuleIds("rules2"));
    chrome.test.succeed();
  },
  async function disableInvalidRulesetRules() {
    // updateStaticRules() must be failed with invalid ruleset id.
    await updateStaticRules({rulesetId: 'invalid_rules', disableRuleIds: [2]},
                            'Error: Invalid ruleset id: invalid_rules.');

    chrome.test.assertEq(['rules1-1', 'rules1-3', 'rules2-1', 'rules2-2'],
                         await getActiveRules());
    chrome.test.assertEq([2], await getDisabledRuleIds("rules1"));
    chrome.test.assertEq([3], await getDisabledRuleIds("rules2"));
    chrome.test.succeed();
  },
  async function disableRulesetRulesExceedingLimits() {
    // updateStaticRules() must be failed when the result exceeds limit.
    await updateStaticRules(
              {rulesetId: 'rules2',
               disableRuleIds: Array.from({length: 5000}, (_, i) => i + 10)},
              'Error: The number of disabled static rules exceeds ' +
                  'the disabled rule count limit.');

    chrome.test.assertEq(['rules1-1', 'rules1-3', 'rules2-1', 'rules2-2'],
                         await getActiveRules());
    chrome.test.assertEq([2], await getDisabledRuleIds("rules1"));
    chrome.test.assertEq([3], await getDisabledRuleIds("rules2"));
    chrome.test.succeed();
  },
  async function updateStaticRulesWithEmptyList() {
    // updateStaticRules() is succeeded with empty rule ids, but doesn't change
    // anything.
    await updateStaticRules({rulesetId: 'rules2'});

    chrome.test.assertEq(['rules1-1', 'rules1-3', 'rules2-1', 'rules2-2'],
                         await getActiveRules());
    chrome.test.assertEq([2], await getDisabledRuleIds("rules1"));
    chrome.test.assertEq([3], await getDisabledRuleIds("rules2"));
    chrome.test.succeed();
  },
  async function getDisabledRuleIdsErrorForInvalidRuleset() {
    await verifyGetDisabledRuleIdsError(
        "invalid_rules", "Error: Invalid ruleset id: invalid_rules.");
    chrome.test.succeed();
  },
]);
