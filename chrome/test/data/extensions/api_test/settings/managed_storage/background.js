// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const stringPolicyName = 'string-policy';
const expectedStringPolicy = { 'string-policy': 'value' };
chrome.test.runTests([
  function getPolicy() {

    function onChangedListener(changes, areaName) {
      chrome.storage.onChanged.removeListener(onChangedListener);
      chrome.test.assertEq(areaName, 'managed');
      chrome.test.assertEq(changes[stringPolicyName]['newValue'],
                           expectedStringPolicy[stringPolicyName]);
      chrome.test.succeed();
    }
    chrome.storage.onChanged.addListener(onChangedListener);

    chrome.storage.managed.get(
        stringPolicyName, function(results) {
          // There can be a race between policy propagation and the start
          // of the tests. If we get an empty value for the results, the
          // installed onChange listener will catch the change and verify
          // it. Otherwise, remove the listener and verify the results.
          if (Object.keys(results).length != 0) {
            chrome.storage.onChanged.removeListener(onChangedListener);
            chrome.test.assertEq(expectedStringPolicy, results);
            chrome.test.succeed();
          }
        });
  },

  // another-string-policy and no-such-thing should not be exposed to the
  // extension: another-string-policy was filled with an int, and no-such-thing
  // just does not exist in the extension's policy schema (see schema.json).
  function getListOfPolicies() {
    chrome.storage.managed.get(
        [
          'string-policy', 'int-policy', 'another-string-policy',
          'no-such-thing'
        ],
        chrome.test.callbackPass(function(results) {
          chrome.test.assertEq({
            'string-policy': 'value',
            'int-policy': -123,
          }, results);
        }));
  },

  function getAllPolicies() {
    chrome.storage.managed.get(
        chrome.test.callbackPass(function(results) {
          chrome.test.assertEq({
            'string-policy': 'value',
            'string-enum-policy': 'value-1',
            'int-policy': -123,
            'int-enum-policy': 1,
            'double-policy': 456e7,
            'boolean-policy': true,
            'list-policy': [ 'one', 'two', 'three' ],
            'dict-policy': {
              'list': [ { 'one': 1, 'two': 2 }, { 'three': 3} ]
            }
          }, results);
        }));
  },

  function getBytesInUse() {
    chrome.storage.managed.getBytesInUse(
        chrome.test.callbackPass(function(bytes) {
          chrome.test.assertEq(0, bytes);
        }));
  },

  function writingFails() {
    var kReadOnlyError = 'This is a read-only store.';
    chrome.storage.managed.clear(chrome.test.callbackFail(kReadOnlyError));
    chrome.storage.managed.remove(
        'string-policy',
        chrome.test.callbackFail(kReadOnlyError));
    chrome.storage.managed.set({
      'key': 'value'
    }, chrome.test.callbackFail(kReadOnlyError));
  }
]);
