// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The id of an extension we're using for install tests.
var extensionId = "enfkhcelefdadlmkffamgdlgplcionje";

// The id of an app we're using for install tests.
var appId = "iladmdjkfniedhfhcfoefgojhgaiaccc";

var assertEq = chrome.test.assertEq;
var assertFalse = chrome.test.assertFalse;
var assertNoLastError = chrome.test.assertNoLastError;
var assertTrue = chrome.test.assertTrue;
var callbackFail = chrome.test.callbackFail;
var callbackPass = chrome.test.callbackPass;
var listenOnce = chrome.test.listenOnce;
var runTests = chrome.test.runTests;
var succeed = chrome.test.succeed;

// Calls |callback| with true/false indicating whether an item with the |id|
// is installed.
function checkItemInstalled(id, callback) {
  chrome.management.getAll(function(extensions) {
    callback(extensions.some(function(ext) {
      return ext.id == id;
    }));
  });
}

// Calls |callback| with true/false indicating whether an item with an id of
// extensionId is installed.
function checkInstalled(callback) {
  checkItemInstalled(extensionId, callback);
}

// This returns the string contents of the extension's manifest file.
function getManifest(alternativePath) {
  // Do a synchronous XHR to get the manifest.
  var xhr = new XMLHttpRequest();
  xhr.open("GET",
           alternativePath ? alternativePath : "extension/manifest.json",
           false);
  xhr.send(null);
  return xhr.responseText;
}

// Installs the extension with the given |installOptions|, calls
// |whileInstalled| and then uninstalls the extension.
function installAndCleanUp(installOptions, whileInstalled) {
  // Begin installing.
  chrome.webstorePrivate.beginInstallWithManifest3(
      installOptions,
      callbackPass(function(result) {
        assertNoLastError();
        assertEq("", result);

        // Now complete the installation.
        chrome.webstorePrivate.completeInstall(
            extensionId,
            callbackPass(function(result) {
              assertNoLastError();
              assertEq(undefined, result);

              whileInstalled();

              chrome.test.runWithUserGesture(callbackPass(function() {
                chrome.management.uninstall(extensionId, {}, callbackPass());
              }));
            }));
      }));
}

// Installs the extension with the given `installOptions`.
function install(installOptions) {
  // Begin installing.
  chrome.webstorePrivate.beginInstallWithManifest3(
      installOptions,
      callbackPass(function(result) {
        assertNoLastError();
        assertEq("", result);

        // Now complete the installation.
        chrome.webstorePrivate.completeInstall(
            extensionId,
            callbackPass(function(result) {
              assertNoLastError();
              assertEq(undefined, result);
            }));
      }));
}
