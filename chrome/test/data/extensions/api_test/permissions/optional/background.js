// Copyright 2012 The Chromium Authors
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
    "Only permissions specified in the manifest may be requested.";

var REQUIRED_ERROR =
    "You cannot remove required permissions.";

var UNKNOWN_PERMISSIONS_ERROR =
    "'*' is not a recognized permission.";

var emptyPermissions = {permissions: [], origins: []};

var initialPermissions = {
  permissions: ['management'],
  origins: ['http://a.com/*', "http://contentscript.com/*"]
};

var permissionsWithBookmarks = {
  permissions: ['management', 'bookmarks'],
  origins: ['http://a.com/*', "http://contentscript.com/*"]
}

var permissionsWithOrigin = {
  permissions: ['management'],
  origins: ['http://a.com/*', 'http://*.c.com/*', "http://contentscript.com/*"]
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

function checkResponse(response) {
  if (response.ok) {
    assertEq(200, response.status);
    return response;
  }
  var error = new Error(response.statusText);
  error.response = response;
  throw error;
}

chrome.test.getConfig(function(config) {

  function doReq(domain, callback) {
    var url = domain + ":PORT/extensions/test_file.txt";
    url = url.replace(/PORT/, config.testServer.port);

    chrome.test.log("Requesting url: " + url);
    fetch(url, {mode: 'no-cors'}).then(checkResponse)
        .then(function(response) {
          return response.text();
        }).then(function(responseText) {
          assertEq("Hello!", responseText);
          callback(true);
        }).catch(function(error) {
          callback(false);
        });
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
            assertThrows(
                chrome.bookmarks.getTree, [function(){}],
                `'bookmarks.getTree' is not available in this context.`);
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

    function unknownPermission() {
      var error_msg = UNKNOWN_PERMISSIONS_ERROR.replace('*', 'asdf');
      chrome.permissions.request(
          {permissions: ['asdf']}, fail(error_msg));
    },

    function requestOrigin() {
      doReq('http://c.com', pass(function(success) {
        assertFalse(success);

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
      }));
    },

    function removeOrigin() {
      doReq('http://c.com', pass(function(result) {
        assertTrue(result);
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
          doReq('http://c.com',
                pass(function(result) { assertFalse(result); }));
        }));
      }));
    },

    // Tests that the changed permissions have taken effect from inside the
    // onAdded and onRemoved event listeners.
    function eventListenerPermissions() {
      var isInstanceOfServiceWorkerGlobalScope =
          ('ServiceWorkerGlobalScope' in self) &&
          (self instanceof ServiceWorkerGlobalScope);
      if (isInstanceOfServiceWorkerGlobalScope) {
        // TODO(crbug.com/40730182): Fix event dispatch ordering for SWs so that
        // permissions.onAdded listener is guaranteed to run *after*
        // chrome.permissions.remove API request below.
        chrome.test.succeed();
        return;
      }
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
        assertThrows(chrome.bookmarks.getTree, [function(){}],
                     `'bookmarks.getTree' is not available in this context.`);
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
