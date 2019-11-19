// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function newAcceletator(keyCode, shift, control, alt, search) {
  var accelerator = new Object();
  accelerator.keyCode = keyCode;
  accelerator.shift = shift ? true : false;
  accelerator.control = control ? true : false;
  accelerator.alt = alt ? true : false;
  accelerator.search = search ? true : false;
  accelerator.pressed = true;
  return accelerator;
};

function sleep(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

var defaultTests = [
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
  // This launches and closes Chrome.
  function launchCloseApp() {
    chrome.autotestPrivate.launchApp('mgndgikekgjfcpckkfioiadnlibdjbkf',
        function() {
          chrome.test.assertNoLastError();
          chrome.autotestPrivate.isAppShown('mgndgikekgjfcpckkfioiadnlibdjbkf',
              function(appShown) {
                chrome.test.assertNoLastError();
                chrome.test.assertTrue(appShown);
                chrome.test.succeed();
               });
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
  // This sets a Crostini app's "scaled" property in the app registry.
  // When the property is set to true, the app will be launched in low display
  // density.
  function setCrostiniAppScaled() {
    chrome.autotestPrivate.setCrostiniAppScaled(
        'nodabfiipdopnjihbfpiengllkohmfkl', true,
        function() {
          chrome.test.assertNoLastError();
          chrome.test.succeed();
        });
  },
  function bootstrapMachineLearningService() {
    chrome.autotestPrivate.bootstrapMachineLearningService(
        chrome.test.callbackFail('ML Service connection error'));
  },
  function runCrostiniUninstaller() {
    chrome.autotestPrivate.runCrostiniUninstaller(chrome.test.callbackFail(
        'Crostini is not available for the current user'));
  },
  function exportCrostini() {
    chrome.autotestPrivate.exportCrostini('backup', chrome.test.callbackFail(
        'Crostini is not available for the current user'));
  },
  function importCrostini() {
    chrome.autotestPrivate.importCrostini('backup', chrome.test.callbackFail(
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
  },
  function setAssistantEnabled() {
    chrome.autotestPrivate.setAssistantEnabled(true, 1000 /* timeout_ms */,
        chrome.test.callbackFail(
            'Assistant not allowed - state: 8'));
  },
  function sendAssistantTextQuery() {
    chrome.autotestPrivate.sendAssistantTextQuery(
        'what time is it?' /* query */,
        1000 /* timeout_ms */,
        chrome.test.callbackFail(
            'Assistant not allowed - state: 8'));
  },
  function waitForAssistantQueryStatus() {
    chrome.autotestPrivate.waitForAssistantQueryStatus(10 /* timeout_s */,
        chrome.test.callbackFail(
            'Assistant not allowed - state: 8'));
  },
  function setWhitelistedPref() {
    chrome.autotestPrivate.setWhitelistedPref(
        'settings.voice_interaction.hotword.enabled' /* pref_name */,
        true /* value */,
        chrome.test.callbackFail(
            'Assistant not allowed - state: 8'));
  },
  // This test verifies that getArcState returns provisioned False in case ARC
  // is not provisioned by default.
  function arcNotProvisioned() {chrome.autotestPrivate.getArcState(
    function(state) {
      chrome.test.assertFalse(state.provisioned);
      chrome.test.assertNoLastError();
      chrome.test.succeed();
    });
  },
  // This test verifies that ARC Terms of Service are needed by default.
  function arcTosNeeded() {
    chrome.autotestPrivate.getArcState(function(state) {
      chrome.test.assertTrue(state.tosNeeded);
      chrome.test.assertNoLastError();
      chrome.test.succeed();
    });
  },
  // No any ARC app by default
  function getArcApp() {
    chrome.autotestPrivate.getArcApp(
         'bifanmfigailifmdhaomnmchcgflbbdn',
         chrome.test.callbackFail('App is not available'));
  },
  // No any ARC package by default
  function getArcPackage() {
    chrome.autotestPrivate.getArcPackage(
         'fake.package',
         chrome.test.callbackFail('Package is not available'));
  },
  // Launch fails, no any ARC app by default
  function launchArcApp() {
    chrome.autotestPrivate.launchArcApp(
        'bifanmfigailifmdhaomnmchcgflbbdn',
        '#Intent;',
        function(appLaunched) {
          chrome.test.assertFalse(appLaunched);
          chrome.test.succeed();
        });
  },
  // This gets the primary display's scale factor.
  function getPrimaryDisplayScaleFactor() {
    chrome.autotestPrivate.getPrimaryDisplayScaleFactor(
        function(scaleFactor) {
          chrome.test.assertNoLastError();
          chrome.test.assertTrue(scaleFactor >= 1.0);
          chrome.test.succeed();
        });
  },
  // This test verifies that api to wait for launcher state transition
  // to the 'Closed' state before creating launcher works.
  function waitForLauncherStateNoChangeBeforeLauncherCreation() {
    chrome.autotestPrivate.waitForLauncherState(
        'Closed',
        function() {
          chrome.test.assertNoLastError();
          chrome.test.succeed();
        });
  },
  // This test verifies that api to wait for launcher state transition
  // works as expected
  function waitForLauncherStatePeeking() {
    var togglePeeking = newAcceletator('search', /*shift=*/false);

    function closeLauncher() {
      togglePeeking.pressed = true;
      chrome.autotestPrivate.activateAccelerator(
          togglePeeking,
          function(success) {
            chrome.test.assertFalse(success);
            togglePeeking.pressed = false;
            chrome.autotestPrivate.activateAccelerator(
                togglePeeking,
                function(success) {
                  chrome.test.assertTrue(success);
                  chrome.autotestPrivate.waitForLauncherState(
                      'Closed',
                      function() {
                        chrome.test.assertNoLastError();
                        chrome.test.succeed();
                      });
                });
          });
    }

    chrome.autotestPrivate.activateAccelerator(
        togglePeeking,
        function(success) {
          chrome.test.assertFalse(success);
          togglePeeking.pressed = false;
          chrome.autotestPrivate.activateAccelerator(
              togglePeeking,
              function(success) {
                chrome.test.assertTrue(success);
                chrome.autotestPrivate.waitForLauncherState(
                    'Peeking',
                    closeLauncher);
              });
        });
  },
  // This test verifies that api to wait for launcher state transition
  // works as expected
  function waitForLauncherStateFullscreen() {
    var toggleFullscreen = newAcceletator('search', /*shift=*/true);
    function closeLauncher() {
      toggleFullscreen.pressed = true;
      chrome.autotestPrivate.activateAccelerator(
          toggleFullscreen,
          function(success) {
            chrome.test.assertFalse(success);
            toggleFullscreen.pressed = false;
            chrome.autotestPrivate.activateAccelerator(
                toggleFullscreen,
                function(success) {
                  chrome.test.assertTrue(success);
                  chrome.autotestPrivate.waitForLauncherState(
                      'Closed',
                      function() {
                        chrome.test.assertNoLastError();
                        chrome.test.succeed();
                      });
                });
          });
    }

    chrome.autotestPrivate.activateAccelerator(
        toggleFullscreen,
        function(success) {
          chrome.test.assertFalse(success);
          toggleFullscreen.pressed = false;
          chrome.autotestPrivate.activateAccelerator(
              toggleFullscreen,
              function(success) {
                chrome.test.assertTrue(success);
                chrome.autotestPrivate.waitForLauncherState(
                    'FullscreenAllApps',
                    closeLauncher);
              });
        });
  },
  // This test verifies that api to wait for launcher state transition
  // to the same 'Closed' state when launcher is in closed state works.
  function waitForLauncherStateNoChangeAfterLauncherCreation() {
    chrome.autotestPrivate.waitForLauncherState(
        'Closed',
        function() {
          chrome.test.assertNoLastError();
          chrome.test.succeed();
        });
  },
  // Check if tablet mode is enabled.
  function isTabletModeEnabled() {
    chrome.autotestPrivate.isTabletModeEnabled(
        function(enabled) {
          chrome.test.assertNoLastError();
          chrome.test.succeed();
        });
  },
  // This test verifies that entering tablet mode works as expected.
  function setTabletModeEnabled() {
    chrome.autotestPrivate.setTabletModeEnabled(true, function(isEnabled){
      chrome.test.assertTrue(isEnabled);
      chrome.test.assertNoLastError();
      chrome.test.succeed();
    });
  },
  // This test verifies that leaving tablet mode works as expected.
  function setTabletModeDisabled() {
    chrome.autotestPrivate.setTabletModeEnabled(false, function(isEnabled){
      chrome.test.assertFalse(isEnabled);
      chrome.test.assertNoLastError();
      chrome.test.succeed();
    });
  },
  // This test verifies that autotetPrivate can correctly query installed apps.
  function getAllInstalledApps() {
    chrome.autotestPrivate.getAllInstalledApps(chrome.test.callbackPass(
      apps => {
        // Limit apps to chromium to filter out default apps.
        const chromium = apps.find(
          app => app.appId == 'mgndgikekgjfcpckkfioiadnlibdjbkf');
        chrome.test.assertTrue(!!chromium);
        // Only check that name and shortName are set for Chromium because
        // their values change if chrome_branded is true.
        chrome.test.assertTrue(!!chromium.name);
        chrome.test.assertTrue(!!chromium.shortName);
        chrome.test.assertEq(chromium.additionalSearchTerms, []);
        chrome.test.assertEq(chromium.readiness, 'Ready');
        chrome.test.assertEq(chromium.showInLauncher, true);
        chrome.test.assertEq(chromium.showInSearch, true);
        chrome.test.assertEq(chromium.type, 'Extension');
    }));
  },
  // This test verifies that only Chromium is available by default.
  function getShelfItems() {
    chrome.autotestPrivate.getShelfItems(chrome.test.callbackPass(items => {
      chrome.test.assertEq(1, items.length);
      item = items[0];
      // Only check that appId and title are set because their values change if
      // chrome_branded is true.
      chrome.test.assertTrue(!!item.appId);
      chrome.test.assertTrue(!!item.title);
      chrome.test.assertEq('', item.launchId);
      chrome.test.assertEq('BrowserShortcut', item.type);
      chrome.test.assertEq('Running', item.status);
      chrome.test.assertTrue(item.showsTooltip);
      chrome.test.assertFalse(item.pinnedByPolicy);
      chrome.test.assertFalse(item.hasNotification);
    }));
  },
  // This test verifies that changing the shelf behavior works as expected.
  function setShelfAutoHideBehavior() {
    // Using shelf from primary display.
    var displayId = "-1";
    chrome.system.display.getInfo(function(info) {
      var l = info.length;
      for (var i = 0; i < l; i++) {
        if (info[i].isPrimary === true) {
          displayId = info[i].id;
          break;
        }
      }
      chrome.test.assertTrue(displayId != "-1");
      // SHELF_AUTO_HIDE_ALWAYS_HIDDEN not supported by shelf_prefs.
      // TODO(ricardoq): Use enums in IDL instead of hardcoded strings.
      var behaviors = ["always", "never"];
      var l = behaviors.length;
      for (var i = 0; i < l; i++) {
        var behavior = behaviors[i];
        chrome.autotestPrivate.setShelfAutoHideBehavior(displayId, behavior,
            function() {
          chrome.test.assertNoLastError();
          chrome.autotestPrivate.getShelfAutoHideBehavior(displayId,
              function(newBehavior) {
            chrome.test.assertNoLastError();
            chrome.test.assertEq(behavior, newBehavior);
          });
        });
      }
      chrome.test.succeed();
    });
  },
  // This test verifies that changing the shelf alignment works as expected.
  function setShelfAlignment() {
    // Using shelf from primary display.
    var displayId = "-1";
    chrome.system.display.getInfo(function(info) {
      var l = info.length;
      for (var i = 0; i < l; i++) {
        if (info[i].isPrimary === true) {
          displayId = info[i].id;
          break;
        }
      }
      chrome.test.assertTrue(displayId != "-1");
      // SHELF_ALIGNMENT_BOTTOM_LOCKED not supported by shelf_prefs.
      var alignments = [chrome.autotestPrivate.ShelfAlignmentType.LEFT,
        chrome.autotestPrivate.ShelfAlignmentType.BOTTOM,
        chrome.autotestPrivate.ShelfAlignmentType.RIGHT,
        chrome.autotestPrivate.ShelfAlignmentType.BOTTOM,]
      var l = alignments.length;
      for (var i = 0; i < l; i++) {
        var alignment = alignments[i];
        chrome.autotestPrivate.setShelfAlignment(displayId, alignment,
            function() {
          chrome.test.assertNoLastError();
          chrome.autotestPrivate.getShelfAlignment(displayId,
              function(newAlignment) {
            chrome.test.assertNoLastError();
            chrome.test.assertEq(newAlignment, alignment);
          });
        });
      }
      chrome.test.succeed();
    });
  },

  function waitForPrimaryDisplayRotation() {
    var displayId = "-1";
    chrome.system.display.getInfo(function(info) {
      var l = info.length;
      for (var i = 0; i < l; i++) {
        if (info[i].isPrimary === true) {
          displayId = info[i].id;
          break;
        }
      }
      chrome.test.assertTrue(displayId != "-1");
      chrome.system.display.setDisplayProperties(displayId, {rotation: 90},
        function() {
          chrome.autotestPrivate.waitForDisplayRotation(displayId, 'Rotate90',
              success => {
                chrome.test.assertNoLastError();
                chrome.test.assertTrue(success);
                // Reset the rotation back to normal.
                chrome.system.display.setDisplayProperties(
                    displayId, {rotation: 0},
                    function() {
                      chrome.autotestPrivate.waitForDisplayRotation(
                          displayId, 'Rotate0',
                          success => {
                            chrome.test.assertNoLastError();
                            chrome.test.assertTrue(success);
                            chrome.test.succeed();
                          });
                    });
              });
        });
    });
  },
  function waitForPrimaryDisplayRotation2() {
    var displayId = "-1";
    chrome.system.display.getInfo(function(info) {
      var l = info.length;
      for (var i = 0; i < l; i++) {
        if (info[i].isPrimary === true) {
          displayId = info[i].id;
          break;
        }
      }
      chrome.test.assertTrue(displayId != "-1");
      chrome.system.display.setDisplayProperties(
          displayId, {rotation: 180},
          function() {
            chrome.autotestPrivate.waitForDisplayRotation(
                displayId, 'Rotate180',
                success => {
                  chrome.test.assertNoLastError();
                  chrome.test.assertTrue(success);
                  // Reset the rotation back to normal.
                  chrome.system.display.setDisplayProperties(
                      displayId, {rotation: 0},
                      function() {
                        chrome.autotestPrivate.waitForDisplayRotation(
                            displayId, 'Rotate0',
                            success => {
                              chrome.test.assertNoLastError();
                              chrome.test.assertTrue(success);
                              chrome.test.succeed();
                            });
                      });
                });
          });
    });
  },
  function arcAppTracingNoArcWindow() {
    chrome.autotestPrivate.arcAppTracingStart(chrome.test.callbackFail(
        'Failed to start custom tracing.'));
  },
  // This test verifies that test can get the window list and set
  // window state.
  function getWindowInfoAndSetState() {
    // Button Masks
    var kMinimizeMask = 1 << 0;
    var kMaximizeRestoreMask = 1 << 1;
    var kCloseMask = 1 << 2;
    var kLeftSnappedMask = 1 << 3;
    var kRightSnappedMask = 1 << 4;

    chrome.autotestPrivate.getAppWindowList(function(list) {
      var browserFrameIndex = -1;
      chrome.test.assertEq(1, list.length);
      for (var i = 0; i < list.length; i++) {
        var window = list[i];
        if (window.windowType != 'Browser') {
          continue;
        }
        browserFrameIndex = i;
        // Sanity check
        chrome.test.assertEq('BrowserFrame', window.name);
        chrome.test.assertTrue(window.title.includes('New Tab') > 0);
        chrome.test.assertEq('Browser', window.windowType);
        chrome.test.assertEq(window.stateType, 'Normal');
        chrome.test.assertTrue(window.isVisible);
        chrome.test.assertTrue(window.targetVisibility);
        chrome.test.assertFalse(window.isAnimating);
        chrome.test.assertTrue(window.canFocus);
        chrome.test.assertTrue(window.hasFocus);
        chrome.test.assertTrue(window.isActive);
        chrome.test.assertFalse(window.hasCapture);
        chrome.test.assertEq(42, window.captionHeight);
        chrome.test.assertEq(
            window.captionButtonVisibleStatus,
            kMinimizeMask | kMaximizeRestoreMask | kCloseMask |
            kLeftSnappedMask | kRightSnappedMask);
        chrome.test.assertEq('Normal', window.frameMode);
        chrome.test.assertTrue(window.isFrameVisible);
        chrome.test.assertFalse(window.hasOwnProperty('overviewInfo'));

        var change = new Object();
        change.eventType = 'WMEventFullscreen';
        chrome.autotestPrivate.setAppWindowState(
            window.id,
            change,
            function(state) {
              chrome.test.assertEq(state, 'Fullscreen');
              chrome.autotestPrivate.getAppWindowList(async function(list) {
                var window = list[browserFrameIndex];
                chrome.test.assertEq('Immersive', window.frameMode);
                chrome.test.assertTrue(window.isFrameVisible);
                // Hide animation finishes in 400ms. Wait 2x for safety.
                await sleep(800);
                chrome.autotestPrivate.getAppWindowList(function(list) {
                  var window = list[browserFrameIndex];
                  chrome.test.assertEq('Immersive', window.frameMode);
                  chrome.test.assertFalse(window.isFrameVisible);
                  // The frame should still have the same buttons.
                  chrome.test.assertEq(
                      window.captionButtonVisibleStatus,
                      kMinimizeMask | kMaximizeRestoreMask | kCloseMask |
                        kLeftSnappedMask | kRightSnappedMask);
                  chrome.test.assertNoLastError();
                  chrome.test.succeed();
                });
              });
            });
      }
      chrome.test.assertTrue(-1 != browserFrameIndex);
    });
  },

  // This test verifies that api to activate accelrator works as expected.
  function acceleratorTest() {
    // Ash level accelerator.
    var newBrowser = newAcceletator('n', /*shift=*/false, /*control=*/true);
    chrome.autotestPrivate.activateAccelerator(
        newBrowser,
        function() {
          chrome.autotestPrivate.getAppWindowList(function(list) {
            chrome.test.assertEq(2, list.length);
            var closeWindow = newAcceletator(
                'w', /*shift=*/false, /*control=*/true);
            chrome.autotestPrivate.activateAccelerator(
                closeWindow,
                function(success) {
                  chrome.test.assertTrue(success);
                  chrome.autotestPrivate.getAppWindowList(function(list) {
                    chrome.test.assertEq(1, list.length);
                    chrome.test.assertNoLastError();
                    chrome.test.succeed();
                  });
                });
          });
        });
  },

  // KEEP |lockScreen()| TESTS AT THE BOTTOM OF THE defaultTests AS IT WILL
  // CHANGE THE SESSION STATE TO LOCKED STATE.
  function lockScreen() {
    chrome.autotestPrivate.lockScreen();
    chrome.test.succeed();
  },
  // ADD YOUR TEST BEFORE |lockScreen()| UNLESS YOU WANT TO RUN TEST IN LOCKED
];

var arcEnabledTests = [
  // This test verifies that getArcState returns provisioned True in case ARC
  // provisioning is done.
  function arcProvisioned() {
    chrome.autotestPrivate.getArcState(function(state) {
        chrome.test.assertTrue(state.provisioned);
        chrome.test.assertNoLastError();
        chrome.test.succeed();
      });
  },
  // This test verifies that ARC Terms of Service are not needed in case ARC is
  // provisioned and Terms of Service are accepted.
  function arcTosNotNeeded() {
    chrome.autotestPrivate.getArcState(function(state) {
      chrome.test.assertFalse(state.tosNeeded);
      chrome.test.assertNoLastError();
      chrome.test.succeed();
    });
  },
  // ARC app is available
  function getArcApp() {
    // bifanmfigailifmdhaomnmchcgflbbdn id is taken from
    //   ArcAppListPrefs::GetAppId(
    //   "fake.package", "fake.package.activity");
    chrome.autotestPrivate.getArcApp('bifanmfigailifmdhaomnmchcgflbbdn',
        chrome.test.callbackPass(function(appInfo) {
          chrome.test.assertNoLastError();
          // See AutotestPrivateArcEnabled for constants.
          chrome.test.assertEq('Fake App', appInfo.name);
          chrome.test.assertEq('fake.package', appInfo.packageName);
          chrome.test.assertEq('fake.package.activity', appInfo.activity);
          chrome.test.assertEq('', appInfo.intentUri);
          chrome.test.assertEq('', appInfo.iconResourceId);
          chrome.test.assertEq(0, appInfo.lastLaunchTime);
          // Install time is set right before this call. Assume we are 5
          // min maximum after setting the install time.
          chrome.test.assertTrue(Date.now() >= appInfo.installTime);
          chrome.test.assertTrue(
              Date.now() <= appInfo.installTime + 5 * 60 * 1000.0);
          chrome.test.assertEq(false, appInfo.sticky);
          chrome.test.assertEq(false, appInfo.notificationsEnabled);
          chrome.test.assertEq(true, appInfo.ready);
          chrome.test.assertEq(false, appInfo.suspended);
          chrome.test.assertEq(true, appInfo.showInLauncher);
          chrome.test.assertEq(false, appInfo.shortcut);
          chrome.test.assertEq(true, appInfo.launchable);

          chrome.test.succeed();
        }));
  },
  // ARC is available but app does not exist
  function getArcNonExistingApp() {
    chrome.autotestPrivate.getArcApp(
        'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa',
        chrome.test.callbackFail('App is not available'));
  },
  // ARC package is available
  function getArcPackage() {
    chrome.autotestPrivate.getArcPackage('fake.package',
        chrome.test.callbackPass(function(packageInfo) {
          chrome.test.assertNoLastError();
          // See AutotestPrivateArcEnabled for constants.
          chrome.test.assertEq('fake.package', packageInfo.packageName);
          chrome.test.assertEq(10, packageInfo.packageVersion);
          chrome.test.assertEq('100', packageInfo.lastBackupAndroidId);
          // Backup time is set right before this call. Assume we are 5
          // min maximum after setting the backup time.
          chrome.test.assertTrue(Date.now() >= packageInfo.lastBackupTime);
          chrome.test.assertTrue(
              Date.now() <= packageInfo.lastBackupTime + 5 * 60 * 1000.0);
          chrome.test.assertEq(true, packageInfo.shouldSync);
          chrome.test.assertEq(false, packageInfo.system);
          chrome.test.assertEq(false, packageInfo.vpnProvider);
          chrome.test.succeed();
        }));
  },
  // Launch existing ARC app
  function launchArcApp() {
    chrome.autotestPrivate.launchArcApp(
        'bifanmfigailifmdhaomnmchcgflbbdn',
        '#Intent;',
        function(appLaunched) {
          chrome.test.assertNoLastError();
          chrome.test.assertTrue(appLaunched);
          chrome.test.succeed();
        });
  },
  // Launch non-existing ARC app
  function launchNonExistingApp() {
    chrome.autotestPrivate.launchArcApp(
        'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa',
        '#Intent;',
        function(appLaunched) {
          chrome.test.assertNoLastError();
          chrome.test.assertFalse(appLaunched);
          chrome.test.succeed();
        });
  },
];

var policyTests = [
  function getAllEnterpricePolicies() {
    chrome.autotestPrivate.getAllEnterprisePolicies(
      chrome.test.callbackPass(function(policydata) {
        chrome.test.assertNoLastError();
        // See AutotestPrivateWithPolicyApiTest for constants.
        var expectedPolicy;
        expectedPolicy =
          {
            "chromePolicies":
              {"AllowDinosaurEasterEgg":
                {"level":"mandatory",
                 "scope":"user",
                 "source":"cloud",
                 "value":true}
              },
            "deviceLocalAccountPolicies":{},
            "extensionPolicies":{}
          }
        chrome.test.assertEq(expectedPolicy, policydata);
        chrome.test.succeed();
      }));
  },
  function refreshEnterprisePolicies() {
    chrome.autotestPrivate.refreshEnterprisePolicies(
      chrome.test.callbackPass(function() {
        chrome.test.succeed();
      })
    );
  },

];

var arcPerformanceTracingTests = [
  function arcAppTracingNormal() {
    chrome.autotestPrivate.arcAppTracingStart(async function() {
      chrome.test.assertNoLastError();
      // We generate 15 frames in test.
      await sleep(250);
      chrome.autotestPrivate.arcAppTracingStopAndAnalyze(
          function(tracing) {
            chrome.test.assertNoLastError();
            chrome.test.assertTrue(tracing.success);
            // FPS is based on real time. Make sure it is positive.
            chrome.test.assertTrue(tracing.fps > 0);
            chrome.test.assertTrue(tracing.fps <= 60.0);
            chrome.test.assertEq(216, Math.trunc(tracing.commitDeviation));
            chrome.test.assertEq(48, Math.trunc(100.0 * tracing.renderQuality));
            chrome.test.succeed();
        });
    });
  },
  function arcAppTracingStopWithoutStart() {
    chrome.autotestPrivate.arcAppTracingStopAndAnalyze(
        function(tracing) {
          chrome.test.assertNoLastError();
          chrome.test.assertFalse(tracing.success);
          chrome.test.assertEq(0, tracing.fps);
          chrome.test.assertEq(0, tracing.commitDeviation);
          chrome.test.assertEq(0, tracing.renderQuality);
          chrome.test.succeed();
        });
  },
  function arcAppTracingDoubleStop() {
    chrome.autotestPrivate.arcAppTracingStart(function() {
      chrome.test.assertNoLastError();
      chrome.autotestPrivate.arcAppTracingStopAndAnalyze(
          function(tracing) {
            chrome.test.assertNoLastError();
            chrome.test.assertTrue(tracing.success);
            chrome.autotestPrivate.arcAppTracingStopAndAnalyze(
                function(tracing) {
                  chrome.test.assertNoLastError();
                  chrome.test.assertFalse(tracing.success);
                  chrome.test.succeed();
              });
        });
    });
  },
];

var overviewTests = [
  function getOverviewInfo() {
    chrome.autotestPrivate.getAppWindowList(
        chrome.test.callbackPass(function(list) {
          chrome.test.assertEq(2, list.length);
          list.forEach(window => {
            chrome.test.assertTrue(window.hasOwnProperty('overviewInfo'));

            var info = window.overviewInfo;
            chrome.test.assertTrue(info.hasOwnProperty('bounds'));
            chrome.test.assertEq(typeof info.bounds, 'object');
            chrome.test.assertTrue(info.bounds.hasOwnProperty('left'));
            chrome.test.assertTrue(info.bounds.hasOwnProperty('top'));
            chrome.test.assertTrue(info.bounds.hasOwnProperty('width'));
            chrome.test.assertTrue(info.bounds.hasOwnProperty('height'));

            chrome.test.assertTrue(info.hasOwnProperty('isDragged'));
            chrome.test.assertEq(false, info.isDragged);
          });
        }));
  }
];

var overviewDragTests = [
  function getOverviewItemInfos() {
    chrome.autotestPrivate.getAppWindowList(
        chrome.test.callbackPass(function(list) {
          var draggedItemCount = 0;
          list.forEach(window => {
            var info = window.overviewInfo;
            chrome.test.assertTrue(info.hasOwnProperty('isDragged'));
            if (info.isDragged)
              ++draggedItemCount;
          });
          chrome.test.assertEq(1, draggedItemCount);
        }));
  }
];

var splitviewLeftSnappedTests = [
  function getSplitViewControllerStateLeftSnapped() {
    chrome.autotestPrivate.getAppWindowList(
        chrome.test.callbackPass(function(list) {
          var found = false;
          list.forEach(window => {
            if (window.stateType == 'LeftSnapped')
              found = true;
          });
          chrome.test.assertTrue(found);
        }));
  }
];

var test_suites = {
  'default': defaultTests,
  'arcEnabled': arcEnabledTests,
  'enterprisePolicies': policyTests,
  'arcPerformanceTracing': arcPerformanceTracingTests,
  'overviewDefault': overviewTests,
  'overviewDrag': overviewDragTests,
  'splitviewLeftSnapped': splitviewLeftSnappedTests
};

chrome.test.getConfig(function(config) {
  var suite = test_suites[config.customArg];
  if (config.customArg in test_suites) {
    chrome.test.runTests(test_suites[config.customArg]);
  } else {
    chrome.test.fail('Invalid test suite');
  }
});
