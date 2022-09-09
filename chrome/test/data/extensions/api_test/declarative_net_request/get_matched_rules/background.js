// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Navigates to |url| and invokes |callback| when the navigation is complete.
function navigateTab(url, callback) {
  chrome.tabs.onUpdated.addListener(function updateCallback(_, info, tab) {
    if (info.status == 'complete' && tab.url == url) {
      chrome.tabs.onUpdated.removeListener(updateCallback);
      callback(tab);
    }
  });

  chrome.tabs.update({url: url});
}

var testServerPort;
function getServerURL(host) {
  if (!testServerPort)
    throw new Error('Called getServerURL outside of runTests.');
  return `http://${host}:${testServerPort}/`;
}

var testData = [
  {host: 'ab.com', rule: {ruleId: 1, rulesetId: 'rules1'}},
  {host: 'abc.com', rule: {ruleId: 2, rulesetId: 'rules1'}},
  {host: 'abcd.com', rule: {ruleId: 1, rulesetId: 'rules2'}},
  {
    host: 'dynamic.com',
    rule:
        {ruleId: 1, rulesetId: chrome.declarativeNetRequest.DYNAMIC_RULESET_ID}
  },
  {
    host: 'session.com',
    rule:
        {ruleId: 5, rulesetId: chrome.declarativeNetRequest.SESSION_RULESET_ID}
  }
];

function addDynamicRule() {
  const rule = {
    id: 1,
    priority: 1,
    condition: {urlFilter: 'dynamic', resourceTypes: ['main_frame']},
    action: {type: 'block'},
  };
  chrome.declarativeNetRequest.updateDynamicRules({addRules: [rule]}, () => {
    chrome.test.assertNoLastError();
    chrome.test.succeed();
  });
}

function addSessionRule() {
  const rule = {
    id: 5,
    priority: 1,
    condition: {urlFilter: 'session', resourceTypes: ['main_frame']},
    action: {type: 'block'},
  };
  chrome.declarativeNetRequest.updateSessionRules({addRules: [rule]}, () => {
    chrome.test.assertNoLastError();
    chrome.test.succeed();
  });
}

function checkTimeStamp(timeStamp) {
  chrome.test.assertTrue(!!timeStamp);

  // Sanity check that the |timeStamp| has a reasonable value i.e. +/- 10
  // minutes from now.
  const minutesToMilliseconds = 60 * 1000;
  const minExpectedTimeStamp =
      new Date(Date.now() - 10 * minutesToMilliseconds);
  const maxExpectedTimeStamp =
      new Date(Date.now() + 10 * minutesToMilliseconds);

  const date = new Date(timeStamp);
  chrome.test.assertTrue(date > minExpectedTimeStamp);
  chrome.test.assertTrue(date < maxExpectedTimeStamp);
}

// Generates a function to verify the |testData| for the given |index|.
function createTest(index) {
  return function() {
    const url = getServerURL(testData[index].host);

    navigateTab(url, (tab) => {
      chrome.declarativeNetRequest.getMatchedRules(
          {tabId: tab.id}, (details) => {
            chrome.test.assertTrue(!!details.rulesMatchedInfo);
            const matchedRules = details.rulesMatchedInfo;

            chrome.test.assertEq(1, matchedRules.length);
            const matchedRule = matchedRules[0];

            checkTimeStamp(matchedRule.timeStamp);
            delete matchedRule.timeStamp;

            const expectedRuleInfo = {
              rule: testData[index].rule,
              tabId: tab.id
            };

            // Sanity check that the RulesMatchedInfo fields are populated
            // correctly.
            chrome.test.assertEq(expectedRuleInfo, matchedRule);

            chrome.test.succeed();
          });
    });
  };
};

chrome.test.getConfig(function(config) {
  testServerPort = config.testServer.port;
  var tests = [];

  // First add the dynamic and session rule, since it's required by one of the
  // latter tests.
  tests.push(addDynamicRule);
  tests.push(addSessionRule);

  for (var i = 0; i < testData.length; ++i) {
    var test = createTest(i);

    // Assign a name to the function so that the extension test framework prints
    // the sub-test name.
    Object.defineProperty(test, 'name', {value: 'test' + i})

    tests.push(test);
  }

  chrome.test.runTests(tests);
});
