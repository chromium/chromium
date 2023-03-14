// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var declarative = chrome.declarative;

var RequestMatcher = chrome.declarativeWebRequest.RequestMatcher;
var CancelRequest = chrome.declarativeWebRequest.CancelRequest;
var RedirectRequest = chrome.declarativeWebRequest.RedirectRequest;
var SetRequestHeader = chrome.declarativeWebRequest.SetRequestHeader;

var inputRule0 = {
  // No 'id', this should be filled by the API.
  conditions: [new RequestMatcher({url: {hostPrefix: "test1"}}),
               new RequestMatcher({url: {hostPrefix: "test2"}})],
  actions: [new CancelRequest(),
            new RedirectRequest({redirectUrl: "http://foobar.com"})]
  // No 'priority', this should be filled by the API.
}

var outputRule0 = {
  id: "_0_",
  conditions: [new RequestMatcher({url: {hostPrefix: "test1"}}),
               new RequestMatcher({url: {hostPrefix: "test2"}})],
  actions: [new CancelRequest(),
            new RedirectRequest({redirectUrl: "http://foobar.com"})],
  priority: 100
}

var inputRule1 = {
  id: "my_rule_id",
  conditions: [],
  actions: [],
  priority: 10
}

var outputRule1 = inputRule1;

var inputRule2 = {
  // No 'id', this should be filled by the API.
  conditions: [new RequestMatcher({url: {hostPrefix: "test3"}})],
  actions: [new CancelRequest()]
  // No 'priority', this should be filled by the API.
}

var outputRule2 = {
  id: "_1_",
  conditions: [new RequestMatcher({url: {hostPrefix: "test3"}})],
  actions: [new CancelRequest()],
  priority: 100
}

var invalidRule0 = {
  conditions: [new RequestMatcher({url: {hostPrefix: "test1"}})]
  // "actions" is missing but not optional.
};

var invalidRule1 = {
  conditions: [new RequestMatcher({url: {hostPrefix: "test1"}})],
  // "actions" contains an invalid action (separate test because this validation
  // happens on a different code path).
  actions: [{key: "value"}]
};
var invalidRule2 = {
  conditions: [new RequestMatcher({url: {hostPrefix: "test1"}})],
  actions: [new SetRequestHeader({name: '\x00', value: 'whatever'})]
};

var testEvent = chrome.declarativeWebRequest.onRequest;

chrome.test.runTests([
  // Test validation
  function testInvalidAddRules() {
    try {
      testEvent.addRules();
      chrome.test.fail();
    } catch(e) {
      chrome.test.succeed();
    }
  },
  function testInvalidGetRules() {
    try {
      testEvent.getRules(1, function() {});
      chrome.test.fail();
    } catch(e) {
      chrome.test.succeed();
    }
  },
  function testInvalidRemoveRules() {
    try {
      testEvent.removeRules(1, function() {});
      chrome.test.fail();
    } catch(e) {
      chrome.test.succeed();
    }
  },
  // Add adding two simple rules and check that their optional fields are set
  // correctly in the call back function.
  function testAddRules() {
    var callback = function(rules) {
      chrome.test.assertNoLastError();
      chrome.test.assertEq(2, rules.length);
      // API should have generated id and priority fields.
      chrome.test.assertTrue("id" in rules[0]);
      chrome.test.assertEq([outputRule0, outputRule1], rules);
      chrome.test.succeed();
    };
    testEvent.addRules([inputRule0, inputRule1], callback);
  },
  // Check that getRules() returns all rules if no filter is passed.
  function testGetRules() {
    var callback = function(rules) {
      chrome.test.assertNoLastError();
      // We are not given any guarantee on the order in which rules are
      // returned.
      chrome.test.assertTrue(
          chrome.test.checkDeepEq([outputRule0, outputRule1], rules) ||
          chrome.test.checkDeepEq([outputRule1, outputRule0], rules));
      chrome.test.succeed();
    }
    testEvent.getRules(null, callback);
  },
  // Check that getRules() returns all rules if no filter is passed.
  function testGetRules2() {
    var callback = function(rules) {
      chrome.test.assertNoLastError();
      // We are not given any guarantee on the order in which rules are
      // returned.
      chrome.test.assertTrue(
          chrome.test.checkDeepEq([outputRule0, outputRule1], rules) ||
          chrome.test.checkDeepEq([outputRule1, outputRule0], rules));
      chrome.test.succeed();
    }
    testEvent.getRules(undefined, callback);
  },
  // Check that getRules() returns no rules if empty filter is passed.
  function testGetRules3() {
    var callback = function(rules) {
      chrome.test.assertNoLastError();
      chrome.test.assertEq([], rules);
      chrome.test.succeed();
    }
    testEvent.getRules([], callback);
  },
  // TODO(devlin): The documentation for event.getRules() states that the
  // filter parameter is optional. However, with JS bindings, we throw an error
  // if it's omitted. This is fixed with native bindings.
  // Check that getRules() returns all rules if the filter is omitted.
  // function testGetRules4() {
  //   var callback = function(rules) {
  //     chrome.test.assertNoLastError();
  //     // We are not given any guarantee on the order in which rules are
  //     // returned.
  //     chrome.test.assertTrue(
  //         chrome.test.checkDeepEq([outputRule0, outputRule1], rules) ||
  //         chrome.test.checkDeepEq([outputRule1, outputRule0], rules));
  //     chrome.test.succeed();
  //   }
  //   testEvent.getRules(callback);
  // },
  // Check that getRules() returns matching rules if rules are filtered by ID.
  function testSelectiveGetRules() {
    var callback = function(rules) {
      chrome.test.assertNoLastError();
      chrome.test.assertEq([outputRule1], rules);
      chrome.test.succeed();
    }
    testEvent.getRules(["my_rule_id"], callback);
  },
  // Check that we can remove individual rules.
  function testSelectiveRemoveRules() {
    var callback = function(rules) {
      chrome.test.assertNoLastError();
      chrome.test.succeed();
    }
    testEvent.removeRules(["my_rule_id"], callback);
  },
  // Check that after removal, the rules are really gone.
  function testGetRemainingRules() {
    var callback = function(rules) {
      chrome.test.assertNoLastError();
      chrome.test.assertEq([outputRule0], rules);
      chrome.test.succeed();
    }
    testEvent.getRules(null, callback);
  },
  // Check that rules are assigned unique IDs.
  function testIdGeneration() {
    var callback = function(rules) {
      chrome.test.assertNoLastError();
      chrome.test.assertEq(1, rules.length);
      // API should have generated id and priority fields.
      chrome.test.assertTrue("id" in rules[0]);
      // The IDs should be distinct.
      chrome.test.assertNe(rules[0]["id"], outputRule0["id"]);
      chrome.test.succeed();
    };
    testEvent.addRules([inputRule2], callback);
  },
  // Check that we can remove all rules at once.
  function testRemovingAllRules() {
    var callback = function() {
      chrome.test.assertNoLastError();
      chrome.test.succeed();
    }
    testEvent.removeRules(null, callback);
  },
  // Check that the rules are actually gone.
  function testAllRulesRemoved() {
    var callback = function(rules) {
      chrome.test.assertNoLastError();
      chrome.test.assertEq(0, rules.length);
      chrome.test.succeed();
    }
    testEvent.getRules(null, callback);
  },
  // Check that validation is performed.
  function testValidation() {
    var fail = function() {
      chrome.test.fail("An exception was expected");
    };
    try {
      testEvent.addRules([invalidRule0], fail);
      fail();
    } catch (e) {}
    try {
      testEvent.addRules([invalidRule1], fail);
      fail();
    } catch (e) {}
    // None of these rules should have been registered.
    var callback = function(rules) {
      chrome.test.assertNoLastError();
      chrome.test.assertEq(0, rules.length);
      chrome.test.succeed();
    };
    testEvent.getRules(null, callback);
  },
  // Check that errors are propagated
  function testValidationAsync() {
    var callback = function() {
      chrome.test.assertLastError('Invalid header name.');
      chrome.test.succeed();
    };
    testEvent.addRules([invalidRule2], callback);
  },
  // Finally we add one additional rule, to check that is is removed
  // on page unload.
  function testAddRules() {
    var callback = function(rules) {
      chrome.test.assertNoLastError();
      chrome.test.assertEq(1, rules.length);
      chrome.test.succeed();
    };
    testEvent.addRules([inputRule0], callback);
  },
  ]);
