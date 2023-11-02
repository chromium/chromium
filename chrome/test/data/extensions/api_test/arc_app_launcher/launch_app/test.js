// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.app.runtime.onLaunched.addListener(function() {
  var expectedPackageName = null;

  chrome.test.runTests([
    function getConfig() {
      chrome.test.getConfig(chrome.test.callbackPass(config => {
        expectedPackageName = config.customArg;
      }));
    },

    function getPackageNameAndLaunchApp() {
      chrome.arcAppsPrivate.launchApp(
          'invalid package name', chrome.test.callbackFail('App not found'));
      chrome.test.assertTrue(!!expectedPackageName);
      chrome.arcAppsPrivate.getLaunchableApps(
          chrome.test.callbackPass(appsInfo => {
            chrome.test.assertEq(
                [{packageName: expectedPackageName}], appsInfo);
            chrome.arcAppsPrivate.launchApp(
                appsInfo[0].packageName, chrome.test.callbackPass());
          }));
    }
  ]);
});
