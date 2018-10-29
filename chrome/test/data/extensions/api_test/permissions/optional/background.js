// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var assertEq = chrome.test.assertEq;
var assertFalse = chrome.test.assertFalse;
var assertTrue = chrome.test.assertTrue;
var assertThrows = chrome.test.assertThrows;
var fail = chrome.test.callbackFail;
var pass = chrome.test.callbackPass;
var listenOnce = chrome.test.listenOnce;

var NOT_OPTIONAL_ERROR =
    "Optional permissions must be listed in extension manifest.";

var REQUIRED_ERROR =
    "You cannot remove required permissions.";

var NOT_WHITE_LISTED_ERROR =
    "The optional permissions API does not support '*'.";

var UNKNOWN_PERMISSIONS_ERROR =
    "'*' is not a recognized permission.";

var emptyPermissions = {permissions: [], origins: []};

var initialPermissions = {
  permissions: ['management'],
  origins: ['http://a.com/*']
};

var permissionsWithBookmarks = {
  permissions: ['management', 'bookmarks'],
  origins: ['http://a.com/*']
}

var permissionsWithOrigin = {
  permissions: ['management'],
  origins: ['http://a.com/*', 'http://*.c.com/*']
}

function checkEqualSets(set1, set2) {
  if (set1.length != set2.length)
    return false;

  for (var x = 0; x < set1.length; x++) {
    if (!set2.some(function(v) { return v == set1[x]; }))
      return false;
  }

  return true;
}

function checkPermSetsEq(set1, set2) {
  return checkEqualSets(set1.permissions, set2.permissions) &&
         checkEqualSets(set1.origins, set2.origins);
}

chrome.test.getConfig(function(config) {

  function doReq(domain, callback) {
    var req = new XMLHttpRequest();
    var url = domain + ":PORT/extensions/test_file.txt";
    url = url.replace(/PORT/, config.testServer.port);

    chrome.test.log("Requesting url: " + url);
    req.open("GET", url, true);

    req.onload = function() {
      assertEq(200, req.status);
      assertEq("Hello!", req.responseText);
      callback(true);
    };

    req.onerror = function() {
      chrome.test.log("status: " + req.status);
      chrome.test.log("text: " + req.responseText);
      callback(false);
    };

    req.send(null);
  }

  chrome.test.runTests([
    function contains() {
      chrome.permissions.contains(
          {permissions: ['management'], origins: ['http://a.com/*']},
          pass(function(result) { assertTrue(result); }));
      chrome.permissions.contains(
          {permissions: ['devtools'], origins: ['http://a.com/*']},
          pass(function(result) { assertFalse(result); }));
      chrome.permissions.contains(
          {permissions: ['management']},
          pass(function(result) { assertTrue(result); }));
      chrome.permissions.contains(
          {permissions: ['management']},
          pass(function(result) { assertTrue(result); }));
    },

    function getAll() {
      chrome.permissions.getAll(pass(function(permissions) {
        assertTrue(checkPermSetsEq(initialPermissions, permissions));
      }));
    },

    // We should get an error when requesting permissions that haven't been
    // defined in "optional_permissions".
    function requestNonOptional() {
      chrome.permissions.request(
          {permissions: ['history']}, fail(NOT_OPTIONAL_ERROR));
      chrome.permissions.request(
          {origins: ['http://*.b.com/*']}, fail(NOT_OPTIONAL_ERROR));
      chrome.permissions.request(
          {permissions: ['history'], origins: ['http://*.b.com/*']},
          fail(NOT_OPTIONAL_ERROR));
    },

    // We should be able to request the bookmarks API since it's in the granted
    // permissions list (see permissions_apitest.cc).
    function requestBookmarks() {
      assertEq(undefined, chrome.bookmarks);
      listenOnce(chrome.permissions.onAdded,
                 function(permissions) {
        assertTrue(permissions.permissions.length == 1);
        assertTrue(permissions.permissions[0] == 'bookmarks');
      });
      chrome.permissions.request(
          {permissions:['bookmarks']},
          pass(function(granted) {
            assertTrue(granted);
            chrome.bookmarks.getTree(pass(function(result) {
              assertTrue(true);
            }));
            chrome.permissions.getAll(pass(function(permissions) {
              assertTrue(checkPermSetsEq(permissionsWithBookmarks,
                                         permissions));
            }));
      }));
    },

    // You can't remove required permissions.
    function removeRequired() {
      chrome.permissions.remove(
          {permissions: ['management']}, fail(REQUIRED_ERROR));
      chrome.permissions.remove(
          {origins: ['http://a.com/*']}, fail(REQUIRED_ERROR));
      chrome.permissions.remove(
          {permissions: ['bookmarks'], origins: ['http://a.com/*']},
          fail(REQUIRED_ERROR));
    },

    // You can remove permissions you don't have (nothing happens).
    function removeNoOp() {
      chrome.permissions.remove(
          {permissions:['background']},
          pass(function(removed) { assertTrue(removed); }));
      chrome.permissions.remove(
          {origins:['http://*.c.com/*']},
          pass(function(removed) { assertTrue(removed); }));
      chrome.permissions.remove(
          {permissions:['background'], origins:['http://*.c.com/*']},
          pass(function(removed) { assertTrue(removed); }));
    },

    function removeBookmarks() {
      chrome.bookmarks.getTree(pass(function(result) {
        assertTrue(true);
      }));
      listenOnce(chrome.permissions.onRemoved,
                 function(permissions) {
        assertTrue(permissions.permissions.length == 1);
        assertTrue(permissions.permissions[0] == 'bookmarks');
      });
      chrome.permissions.remove(
          {permissions:['bookmarks']},
          pass(function() {
            chrome.permissions.getAll(pass(function(permissions) {
              assertTrue(checkPermSetsEq(initialPermissions, permissions));
            }));
            assertTrue(typeof chrome.bookmarks == 'object' &&
                       chrome.bookmarks != null);
            var nativeBindingsError =
                "'bookmarks.getTree' is not available in this context.";
            var jsBindingsError =
                "'bookmarks' requires a different Feature that is not present.";
            var regexp =
                new RegExp(nativeBindingsError + '|' + jsBindingsError);
            assertThrows(chrome.bookmarks.getTree, [function(){}], regexp);
          }
      ));
    },

    // The user shouldn't have to approve permissions that have no warnings.
    function noPromptForNoWarnings() {
      chrome.permissions.request(
          {permissions: ['cookies']},
          pass(function(granted) {
        assertTrue(granted);

        // Remove the cookies permission to return to normal.
        chrome.permissions.remove(
            {permissions: ['cookies']},
            pass(function(removed) { assertTrue(removed); }));
      }));
    },

    // Make sure you can only access the white listed permissions.
    function whitelist() {
      var error_msg = NOT_WHITE_LISTED_ERROR.replace('*', 'cloudPrintPrivate');
      chrome.permissions.request(
          {permissions: ['cloudPrintPrivate']}, fail(error_msg));
      chrome.permissions.remove(
          {permissions: ['cloudPrintPrivate']}, fail(error_msg));
    },

    function unknownPermission() {
      var error_msg = UNKNOWN_PERMISSIONS_ERROR.replace('*', 'asdf');
      chrome.permissions.request(
          {permissions: ['asdf']}, fail(error_msg));
    },

    function requestOrigin() {
      doReq('http://c.com', pass(function(success) { assertFalse(success); }));

      chrome.permissions.getAll(pass(function(permissions) {
        assertTrue(checkPermSetsEq(initialPermissions, permissions));
      }));

      listenOnce(chrome.permissions.onAdded,
                 function(permissions) {
        assertTrue(permissions.permissions.length == 0);
        assertTrue(permissions.origins.length == 1);
        assertTrue(permissions.origins[0] == 'http://*.c.com/*');
      });
      chrome.permissions.request(
          {origins: ['http://*.c.com/*']},
          pass(function(granted) {
        assertTrue(granted);
        chrome.permissions.getAll(pass(function(permissions) {
          assertTrue(checkPermSetsEq(permissionsWithOrigin, permissions));
        }));
        chrome.permissions.contains(
            {origins:['http://*.c.com/*']},
            pass(function(result) { assertTrue(result); }));
        doReq('http://c.com', pass(function(result) { assertTrue(result); }));
      }));
    },

    function removeOrigin() {
      doReq('http://c.com', pass(function(result) { assertTrue(result); }));

      listenOnce(chrome.permissions.onRemoved,
                 function(permissions) {
        assertTrue(permissions.permissions.length == 0);
        assertTrue(permissions.origins.length == 1);
        assertTrue(permissions.origins[0] == 'http://*.c.com/*');
      });
      chrome.permissions.remove(
          {origins: ['http://*.c.com/*']},
          pass(function(removed) {
        assertTrue(removed);
        chrome.permissions.getAll(pass(function(permissions) {
          assertTrue(checkPermSetsEq(initialPermissions, permissions));
        }));
        chrome.permissions.contains(
            {origins:['http://*.c.com/*']},
            pass(function(result) { assertFalse(result); }));
        doReq('http://c.com', pass(function(result) { assertFalse(result); }));
      }));
    },

    // Tests that the changed permissions have taken effect from inside the
    // onAdded and onRemoved event listeners.
    function eventListenerPermissions() {
      listenOnce(chrome.permissions.onAdded,
                 function(permissions) {
        chrome.bookmarks.getTree(pass(function() {
          assertTrue(true);
        }));
      });
      listenOnce(chrome.permissions.onRemoved,
                 function(permissions) {
        assertTrue(typeof chrome.bookmarks == 'object' &&
                   chrome.bookmarks != null);
        var nativeBindingsError =
            "'bookmarks.getTree' is not available in this context.";
        var jsBindingsError =
            "'bookmarks' requires a different Feature that is not present.";
        var regexp = new RegExp(nativeBindingsError + '|' + jsBindingsError);
        assertThrows(chrome.bookmarks.getTree, [function(){}], regexp);
      });

      chrome.permissions.request(
          {permissions: ['bookmarks']}, pass(function(granted) {
        assertTrue(granted);
        chrome.permissions.remove(
            {permissions: ['bookmarks']}, pass(function() {
          assertTrue(true);
        }));
      }));
    }

  ]);
});
