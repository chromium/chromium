// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var tests = [
  function appInstallBubble() {
    // See things through all the way to a successful install.
    listenOnce(chrome.management.onInstalled, function(info) {
      assertEq(info.id, appId);
    });

    var manifest = getManifest("app/manifest.json");
    // Begin installing.
    chrome.webstorePrivate.beginInstallWithManifest3(
        {'id': appId,'manifest': manifest, 'appInstallBubble':true},
        callbackPass(function(result) {
      assertEq(result, "");

      // Now complete the installation.
      chrome.webstorePrivate.completeInstall(appId, callbackPass());
    }));
  }
];

runTests(tests);
