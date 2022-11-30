// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var ERROR = 'Only permissions specified in the manifest may be requested.';
var test = chrome.test;

// The URL patterns that we've supposedly been granted access to so far. Use
// this to verify queried hosts for each test.
var grantedHosts = [];

// Pushes a value onto an array, asserting that it's unique in the array.
function pushUniqueValue(array, value) {
  test.assertEq(-1, array.indexOf(value));
  array.push(value);
}

// Removes a value from an array, asserting that it's unique in the array.
function removeUniqueValue(array, value) {
  var indexOfValue = array.indexOf(value);
  test.assertTrue(indexOfValue >= 0);
  array.splice(indexOfValue, 1);
  test.assertEq(-1, array.indexOf(value));
}

// Asserts that two arrays are equal when treated as sets.
function assertSetEq(expected, actual) {
  if (!actual)
    actual = [];
  test.assertEq(expected.slice().sort(), actual.slice().sort());
}

// Validates that the origin permissions returned from getAll() and contains()
// match |grantedHosts|.
function checkGrantedHosts(callback) {
  chrome.permissions.getAll(test.callbackPass(function(permissions) {
    assertSetEq(grantedHosts, permissions.origins);
    var countDown = grantedHosts.length;
    if (countDown == 0) {
      callback();
      return;
    }
    grantedHosts.forEach(function(host) {
      chrome.permissions.contains({origins: [host]},
                                  test.callbackPass(function(contains) {
        test.assertTrue(contains);
        if (--countDown == 0)
          callback();
      }));
    });
  }));
}

// Returns a function which requests access to a host, and afterwards checks
// that our expected host permissions agree with Chrome's.
function requestHost(host, expectedGranted, expectedError) {
  return function(callback) {
    chrome.permissions.request({origins: [host]},
                               test.callback(function(granted) {
      if (granted)
        pushUniqueValue(grantedHosts, host);
      if (expectedGranted) {
        test.assertTrue(
            granted,
            "Access to " + host + " was not granted, but should have been");
      } else {
        test.assertFalse(
            !!granted,
            "Access to " + host + " was granted, but should not have been");
      }
      checkGrantedHosts(callback);
    }, expectedError));
  };
}

// Returns a function which removes access to a host, and afterwards checks
// that our expected host permissions agree with Chrome's.
function removeHost(host, expectedRemoved) {
  return function(callback) {
    chrome.permissions.remove({origins: [host]},
                              test.callbackPass(function(removed) {
      if (removed) {
        test.assertTrue(expectedRemoved, "Access to " + host + " removed");
        removeUniqueValue(grantedHosts, host);
      } else {
        test.assertFalse(expectedRemoved, "Access to " + host + " not removed");
      }
      checkGrantedHosts(callback);
    }));
  }
}

// Returns a function which checks that permissions.contains(host) returns
// |expected|.
function contains(host, expected) {
  return function(callback) {
    chrome.permissions.contains({origins: [host]},
                                test.callbackPass(function(result) {
      test.assertEq(expected, result);
      callback();
    }));
  };
}

// Calls every function in |chain| passing each the next function to run as a
// single argument - except for the last function, which is *not* expected to
// have a callback.
//
// The test is kept alive until all callbacks in the chain have been run.
function chain(callbacks) {
  var head = callbacks[0], tail = callbacks.slice(1);
  if (tail.length == 0) {
    head();
    return;
  }
  var callbackCompleted = chrome.test.callbackAdded();
  head(function() {
    try {
      chain(tail);
    } finally {
      callbackCompleted();
    }
  });
}

function main() {
  chain([
                 // Simple request #1.
        contains('http://google.com/*', false),
     requestHost('http://google.com/*', true),
        contains('http://google.com/*', true),
        contains('http://google.com/foo/*', true),
        contains('http://google.com:1234/foo/*', true),
        contains('http://www.google.com/*', false),

                 // Simple request #2.
     requestHost('http://*.yahoo.com/*', true),
        contains('http://yahoo.com/*', true),
        contains('http://yahoo.com/foo/*', true),
        contains('http://www.yahoo.com/*', true),
        contains('http://www.yahoo.com/foo/*', true),
        contains('http://www.yahoo.com:5678/foo/*', true),

                 // Can request access to a subset of a host that's allowed.
     requestHost('http://yahoo.com/*', true),

                 // Can remove it.
      removeHost('http://yahoo.com/*', true),

                 // However, since it's still granted by the *.yahoo.com/*
                 // permission, it's still considered to contain it.
        contains('http://yahoo.com/*', true),

                 // Can remove the whole host, though.
      removeHost('http://*.yahoo.com/*', true),
        contains('http://google.com/*', true),
        contains('http://yahoo.com/*', false),
        contains('http://www.yahoo.com/*', false),

                 // Widening the net (and then deleting it).
     requestHost('http://*.google.com/*', true),
        contains('http://google.com/*', true),
        contains('http://www.google.com/*', true),
      removeHost('http://google.com/*', true),
        contains('http://google.com/*', true),
        contains('http://www.google.com/*', true),
      removeHost('http://*.google.com/*', true),
        contains('http://google.com/*', false),
        contains('http://www.google.com/*', false),

                 // https schemes should fail because they're not covered by
                 // this extension's optional permission pattern.
     requestHost('https://google.com/*', false, ERROR),
     requestHost('https://*.yahoo.com/*', false, ERROR),

                 // There is a sneaky ftp://ftp.example.com/* pattern in the
                 // manifest. Test it.
     requestHost('ftp://example.com/*', false, ERROR),
        contains('ftp://example.com/*', false),
     requestHost('ftp://www.example.com/*', false, ERROR),
        contains('ftp://www.example.com/*', false),
     requestHost('ftp://secret.ftp.example.com/*', false, ERROR),
        contains('ftp://secret.ftp.example.com/*', false),
     requestHost('ftp://ftp.example.com/*', true),
        contains('ftp://ftp.example.com/*', true),
      removeHost('ftp://ftp.example.com/*', true),
        contains('ftp://ftp.example.com/*', false),

                 // And finally...
                 test.succeed]);
}

test.runTests([main]);
