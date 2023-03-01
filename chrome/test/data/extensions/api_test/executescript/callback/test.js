// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var relativePath =
    '/extensions/api_test/executescript/callback/test.html';
var testUrl = 'http://b.com:PORT' + relativePath;

chrome.test.getConfig(function(config) {
  testUrl = testUrl.replace(/PORT/, config.testServer.port);
  chrome.tabs.onUpdated.addListener(function(tabId, changeInfo, tab) {
    if (changeInfo.status != 'complete')
      return;
    chrome.tabs.onUpdated.removeListener(arguments.callee);
    chrome.test.runTests([

      function executeCallbackIntShouldSucceed() {
        var scriptDetails = {code: '3'};
        chrome.tabs.executeScript(tabId, scriptDetails, function(scriptVal) {
          chrome.tabs.get(tabId, chrome.test.callbackPass(function(tab) {
            chrome.test.assertEq(3, scriptVal[0]);
          }));
        });
      },

      function executeCallbackDoubleShouldSucceed() {
        var scriptDetails = {code: '1.4'};
        chrome.tabs.executeScript(tabId, scriptDetails, function(scriptVal) {
          chrome.tabs.get(tabId, chrome.test.callbackPass(function(tab) {
            chrome.test.assertEq(1.4, scriptVal[0]);
          }));
        });
      },

      function executeCallbackStringShouldSucceed() {
        var scriptDetails = {code: '"foobar"'};
        chrome.tabs.executeScript(tabId, scriptDetails, function(scriptVal) {
          chrome.tabs.get(tabId, chrome.test.callbackPass(function(tab) {
            chrome.test.assertEq('foobar', scriptVal[0]);
          }));
        });
      },

      function executeCallbackTrueShouldSucceed() {
        var scriptDetails = {code: 'true'};
        chrome.tabs.executeScript(tabId, scriptDetails, function(scriptVal) {
          chrome.tabs.get(tabId, chrome.test.callbackPass(function(tab) {
            chrome.test.assertEq(true, scriptVal[0]);
          }));
        });
      },

      function executeCallbackFalseShouldSucceed() {
        var scriptDetails = {code: 'false'};
        chrome.tabs.executeScript(tabId, scriptDetails, function(scriptVal) {
          chrome.tabs.get(tabId, chrome.test.callbackPass(function(tab) {
            chrome.test.assertEq(false, scriptVal[0]);
          }));
        });
      },

      function executeCallbackNullShouldSucceed() {
        var scriptDetails = {code: 'null'};
        chrome.tabs.executeScript(tabId, scriptDetails, function(scriptVal) {
          chrome.tabs.get(tabId, chrome.test.callbackPass(function(tab) {
            chrome.test.assertEq(null, scriptVal[0]);
          }));
        });
      },

      function executeCallbackArrayShouldSucceed() {
        var scriptDetails = {code: '[1, "5", false, null]'};
        chrome.tabs.executeScript(tabId, scriptDetails, function(scriptVal) {
          chrome.tabs.get(tabId, chrome.test.callbackPass(function(tab) {
            chrome.test.assertEq([1, "5", false, null], scriptVal[0]);
          }));
        });
      },

      function executeCallbackObjShouldSucceed() {
        var scriptDetails = {code: 'var obj = {"id": "foo", "bar": 9}; obj'};
        chrome.tabs.executeScript(tabId, scriptDetails, function(scriptVal) {
          chrome.tabs.get(tabId, chrome.test.callbackPass(function(tab) {
            chrome.test.assertEq({"id": "foo", "bar": 9}, scriptVal[0]);
          }));
        });
      },

      // DOM objects (nodes, properties, etc) should be converted to empty
      // objects. We could try to convert them the best they can but it's
      // undefined what that means. Ideally it'd just throw an exception but
      // the backwards compatible ship sailed long ago.
      function executeCallbackDOMObjShouldSucceedAndReturnNull() {
        [ 'document',
          'document.getElementById("testDiv")',
          'new XMLHttpRequest()',
          'document.location',
          'navigator',
        ].forEach(function(expr) {
          chrome.tabs.executeScript(tabId,
                                    {code: 'var obj = ' + expr + '; obj'},
                                    chrome.test.callbackPass(function(result) {
            chrome.test.assertEq([{}], result, 'Failed for ' + expr);
          }));
        });
      },

      // All non-integer properties are droped.
      function executeCallbackArrayWithNonNumericFieldsShouldSucceed() {
        var scriptDetails = {}
        scriptDetails.code = 'var arr = [1, 2]; arr.foo = "bar"; arr;';
        chrome.tabs.executeScript(tabId, scriptDetails, function(scriptVal) {
          chrome.tabs.get(tabId, chrome.test.callbackPass(function(tab) {
            chrome.test.assertEq([1, 2], scriptVal[0]);
          }));
        });
      },

      function executeCallbackObjWithNumericFieldsShouldSucceed() {
        var scriptDetails = {}
        scriptDetails.code = 'var obj = {1: 1, "2": "a", "foo": "bar"}; obj;';
        chrome.tabs.executeScript(tabId, scriptDetails, function(scriptVal) {
          chrome.tabs.get(tabId, chrome.test.callbackPass(function(tab) {
            chrome.test.assertEq({'foo': 'bar', 1: 1, '2': 'a'}, scriptVal[0]);
          }));
        });
      },

      function executeCallbackRecursiveObjShouldSucceed() {
        var scriptDetails = {code: 'var foo = {"a": 1}; foo.bar = foo; foo;'};
        chrome.tabs.executeScript(tabId, scriptDetails, function(scriptVal) {
          chrome.tabs.get(tabId, chrome.test.callbackPass(function(tab) {
            chrome.test.assertEq({'a': 1, 'bar': null}, scriptVal[0]);
          }));
        });
      },

      function executeCallbackRecursiveArrayShouldSucceed() {
        var scriptDetails =
            {code: 'var arr = [1, "2", 3.4]; arr.push(arr); arr;'};
        chrome.tabs.executeScript(tabId, scriptDetails, function(scriptVal) {
          chrome.tabs.get(tabId, chrome.test.callbackPass(function(tab) {
            chrome.test.assertEq([1, "2", 3.4, null], scriptVal[0]);
          }));
        });
      },

      function executeCallbackWindowShouldSucceed() {
        var scriptDetails = {code: 'window;'};
        chrome.tabs.executeScript(tabId, scriptDetails, function(scriptVal) {
          chrome.tabs.get(tabId, chrome.test.callbackPass(function(tab) {
            // Test passes as long as the window was converted in some form and
            // is not null
            chrome.test.assertNe(scriptVal[0], null);
          }));
        });
      },
    ]);
  });
  chrome.tabs.create({ url: testUrl });

});
