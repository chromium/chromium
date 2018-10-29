// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  // logout/restart/shutdown don't do anything as we don't want to kill the
  // browser with these tests.
  function logout() {
    chrome.autotestPrivate.logout();
    chrome.test.succeed();
  },
  function restart() {
    chrome.autotestPrivate.restart();
    chrome.test.succeed();
  },
  function shutdown() {
    chrome.autotestPrivate.shutdown(true);
    chrome.test.succeed();
  },
  function lockScreen() {
    chrome.autotestPrivate.lockScreen();
    chrome.test.succeed();
  },
  function simulateAsanMemoryBug() {
    chrome.autotestPrivate.simulateAsanMemoryBug();
    chrome.test.succeed();
  },
  function loginStatus() {
    chrome.autotestPrivate.loginStatus(
        chrome.test.callbackPass(function(status) {
          chrome.test.assertEq(typeof status, 'object');
          chrome.test.assertTrue(status.hasOwnProperty("isLoggedIn"));
          chrome.test.assertTrue(status.hasOwnProperty("isOwner"));
          chrome.test.assertTrue(status.hasOwnProperty("isScreenLocked"));
          chrome.test.assertTrue(status.hasOwnProperty("isRegularUser"));
          chrome.test.assertTrue(status.hasOwnProperty("isGuest"));
          chrome.test.assertTrue(status.hasOwnProperty("isKiosk"));
          chrome.test.assertTrue(status.hasOwnProperty("email"));
          chrome.test.assertTrue(status.hasOwnProperty("displayEmail"));
          chrome.test.assertTrue(status.hasOwnProperty("userImage"));
        }));
  },
  function getExtensionsInfo() {
    chrome.autotestPrivate.getExtensionsInfo(
        chrome.test.callbackPass(function(extInfo) {
          chrome.test.assertEq(typeof extInfo, 'object');
          chrome.test.assertTrue(extInfo.hasOwnProperty('extensions'));
          chrome.test.assertTrue(extInfo.extensions.constructor === Array);
          for (var i = 0; i < extInfo.extensions.length; ++i) {
            var extension = extInfo.extensions[i];
            chrome.test.assertTrue(extension.hasOwnProperty('id'));
            chrome.test.assertTrue(extension.hasOwnProperty('version'));
            chrome.test.assertTrue(extension.hasOwnProperty('name'));
            chrome.test.assertTrue(extension.hasOwnProperty('publicKey'));
            chrome.test.assertTrue(extension.hasOwnProperty('description'));
            chrome.test.assertTrue(extension.hasOwnProperty('backgroundUrl'));
            chrome.test.assertTrue(extension.hasOwnProperty(
                'hostPermissions'));
            chrome.test.assertTrue(
                extension.hostPermissions.constructor === Array);
            chrome.test.assertTrue(extension.hasOwnProperty(
                'effectiveHostPermissions'));
            chrome.test.assertTrue(
                extension.effectiveHostPermissions.constructor === Array);
            chrome.test.assertTrue(extension.hasOwnProperty(
                'apiPermissions'));
            chrome.test.assertTrue(
                extension.apiPermissions.constructor === Array);
            chrome.test.assertTrue(extension.hasOwnProperty('isComponent'));
            chrome.test.assertTrue(extension.hasOwnProperty('isInternal'));
            chrome.test.assertTrue(extension.hasOwnProperty(
                'isUserInstalled'));
            chrome.test.assertTrue(extension.hasOwnProperty('isEnabled'));
            chrome.test.assertTrue(extension.hasOwnProperty(
                'allowedInIncognito'));
            chrome.test.assertTrue(extension.hasOwnProperty('hasPageAction'));
          }
        }));
  },
  function setTouchpadSensitivity() {
    chrome.autotestPrivate.setTouchpadSensitivity(3);
    chrome.test.succeed();
  },
  function setTapToClick() {
    chrome.autotestPrivate.setTapToClick(true);
    chrome.test.succeed();
  },
  function setThreeFingerClick() {
    chrome.autotestPrivate.setThreeFingerClick(true);
    chrome.test.succeed();
  },
  function setTapDragging() {
    chrome.autotestPrivate.setTapDragging(false);
    chrome.test.succeed();
  },
  function setNaturalScroll() {
    chrome.autotestPrivate.setNaturalScroll(true);
    chrome.test.succeed();
  },
  function setMouseSensitivity() {
    chrome.autotestPrivate.setMouseSensitivity(3);
    chrome.test.succeed();
  },
  function setPrimaryButtonRight() {
    chrome.autotestPrivate.setPrimaryButtonRight(false);
    chrome.test.succeed();
  },
  function setMouseReverseScroll() {
    chrome.autotestPrivate.setMouseReverseScroll(true);
    chrome.test.succeed();
  },
  function getVisibleNotifications() {
    chrome.autotestPrivate.getVisibleNotifications(function(){});
    chrome.test.succeed();
  },
  // In this test, ARC is available but not managed and not enabled by default.
  function getPlayStoreState() {
    chrome.autotestPrivate.getPlayStoreState(function(state) {
      chrome.test.assertTrue(state.allowed);
      chrome.test.assertFalse(state.enabled);
      chrome.test.assertFalse(state.managed);
      chrome.test.succeed();
    });
  },
  // This test turns ARC enabled state to ON.
  function setPlayStoreEnabled() {
    chrome.autotestPrivate.setPlayStoreEnabled(true, function() {
      chrome.test.assertNoLastError();
      chrome.autotestPrivate.getPlayStoreState(function(state) {
        chrome.test.assertTrue(state.allowed);
        chrome.test.assertTrue(state.enabled);
        chrome.test.assertFalse(state.managed);
        chrome.test.succeed();
      });
    });
  },
  function getHistogramExists() {
    // Request an arbitrary histogram that is reported once at startup and seems
    // unlikely to go away.
    chrome.autotestPrivate.getHistogram(
        "Startup.BrowserProcessImpl_PreMainMessageLoopRunTime",
        chrome.test.callbackPass(function(histogram) {
          chrome.test.assertEq(typeof histogram, 'object');
          chrome.test.assertEq(histogram.buckets.length, 1);
          chrome.test.assertEq(histogram.buckets[0].count, 1);
          chrome.test.assertTrue(
              histogram.buckets[0].max > histogram.buckets[0].min);
        }));
  },
  function getHistogramMissing() {
    chrome.autotestPrivate.getHistogram(
        'Foo.Nonexistent',
        chrome.test.callbackFail('Histogram Foo.Nonexistent not found'));
  },
  // This test verifies that Play Store window is not shown by default but
  // Chrome is shown.
  function isAppShown() {
    chrome.autotestPrivate.isAppShown('cnbgggchhmkkdmeppjobngjoejnihlei',
        function(appShown) {
          chrome.test.assertFalse(appShown);
          chrome.test.assertNoLastError();

          // Chrome is running.
          chrome.autotestPrivate.isAppShown('mgndgikekgjfcpckkfioiadnlibdjbkf',
              function(appShown) {
                 chrome.test.assertTrue(appShown);
                 chrome.test.assertNoLastError();
                 chrome.test.succeed();
            });
        });
  },
  // This launches Chrome.
  function launchApp() {
    chrome.autotestPrivate.launchApp('mgndgikekgjfcpckkfioiadnlibdjbkf',
      function() {
        chrome.test.assertNoLastError();
        chrome.test.succeed();
      });
  },
  function setCrostiniEnabled() {
    chrome.autotestPrivate.setCrostiniEnabled(true, chrome.test.callbackFail(
        'Crostini is not available for the current user'));
  },
  function runCrostiniInstaller() {
    chrome.autotestPrivate.runCrostiniInstaller(chrome.test.callbackFail(
        'Crostini is not available for the current user'));
  },
  function bootstrapMachineLearningService() {
    chrome.autotestPrivate.bootstrapMachineLearningService(
        chrome.test.callbackFail('ML Service connection error'));
  },
  function runCrostiniUninstaller() {
    chrome.autotestPrivate.runCrostiniUninstaller(chrome.test.callbackFail(
        'Crostini is not available for the current user'));
  },
  function takeScreenshot() {
    chrome.autotestPrivate.takeScreenshot(
      function(base64Png) {
        chrome.test.assertTrue(base64Png.length > 0);
        chrome.test.assertNoLastError();
        chrome.test.succeed();
      }
    )
  },
  function getPrinterList() {
    chrome.autotestPrivate.getPrinterList(function(){
      chrome.test.succeed();
    });
  }
]);
