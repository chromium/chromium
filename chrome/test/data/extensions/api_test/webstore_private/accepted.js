// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests where the beginInstallWithManifest3 dialog would be auto-accepted
// (including a few cases where this does not matter).

var tests = [

  function completeBeforeBegin() {
    var expectedError = extensionId +
        " does not match a previous call to beginInstallWithManifest3";
    chrome.webstorePrivate.completeInstall(extensionId,
                                           callbackFail(expectedError));
  },

  function invalidID() {
    var expectedError = "Invalid id";
    var id = "zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz";
    chrome.webstorePrivate.beginInstallWithManifest3(
        { 'id':id, 'manifest':getManifest() }, callbackFail(expectedError));
  },

  function missingVersion() {
    var manifestObj = JSON.parse(getManifest());
    delete manifestObj["version"];
    var manifest = JSON.stringify(manifestObj);
    chrome.webstorePrivate.beginInstallWithManifest3(
        { 'id':extensionId, 'manifest': manifest },
        callbackFail("Invalid manifest", function(result) {
      assertEq("manifest_error", result);
    }));
  },

  function successfulInstall() {
    // See things through all the way to a successful install.
    listenOnce(chrome.management.onInstalled, function(info) {
      assertEq(info.id, extensionId);
    });

    var manifest = getManifest();
    installAndCleanUp(
        {'id': extensionId, 'iconUrl': 'extension/icon.png',
         'manifest': manifest},
         function() {}
    );
  },

  function duplicateInstall() {
    // See things through all the way to a successful install.
    listenOnce(chrome.management.onInstalled, function(info) {
      assertEq(info.id, extensionId);
    });

    var manifest = getManifest();
    iconUrl = 'extension/icon.png';
    installAndCleanUp(
        {'id': extensionId, 'iconUrl': iconUrl, 'manifest': manifest},
        function() {
          // Kick off a serial second install. This should fail.
          var expectedError = "This item is already installed";
          chrome.webstorePrivate.beginInstallWithManifest3(
              {'id': extensionId, 'iconUrl': iconUrl, 'manifest': manifest},
              callbackFail(expectedError));
        });

    // Kick off a simultaneous second install. This should fail.
    var expectedError = "This item is already installed";
    chrome.webstorePrivate.beginInstallWithManifest3(
        {'id': extensionId, 'iconUrl': iconUrl, 'manifest': manifest},
        callbackFail(expectedError));
  }
];

runTests(tests);
