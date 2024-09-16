// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function newAccelerator(keyCode, shift, control, alt, search) {
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

function promisify(f, ...args) {
  return new Promise((resolve, reject) => {
    f(...args, (result) => {
      if (chrome.runtime.lastError) {
        reject(chrome.runtime.lastError);
      } else {
        resolve(result);
      }
    });
  });
}

function closeLauncher(closeCallback) {
  var toggle = newAccelerator('search', false /* shift */);
  toggle.pressed = true;
  chrome.autotestPrivate.activateAccelerator(
      toggle, chrome.test.callbackPass(function(success) {
        chrome.test.assertFalse(success);
        toggle.pressed = false;
        chrome.autotestPrivate.activateAccelerator(
            toggle, chrome.test.callbackPass(function(success) {
              chrome.test.assertTrue(success);
              chrome.autotestPrivate.waitForLauncherState(
                  'Closed', chrome.test.callbackPass(closeCallback));
            }));
      }));
}

// Minimizes the browser window while testing tablet mode launcher, so the
// launcher actually gets shown when entering tablet mode.
function minimizeBrowserWindow(callback) {
  chrome.autotestPrivate.getAppWindowList(function(list) {
    chrome.test.assertNoLastError();
    chrome.test.assertEq(1, list.length);
    var browser = list[0];
    chrome.test.assertEq('Browser', browser.windowType);
    chrome.test.assertEq('Normal', browser.stateType);
    chrome.autotestPrivate.setAppWindowState(
        browser.id, {eventType: 'WMEventMinimize'}, true /* wait */,
        function(state) {
          chrome.test.assertNoLastError();
          chrome.test.assertEq('Minimized', state);
          callback();
        });
  });
}

// Unminimizes the browser window that was minimized for tests that were using
// tablet mode launcher.
function unminimizeBrowserWindow(callback) {
  chrome.autotestPrivate.getAppWindowList(function(list) {
    chrome.test.assertNoLastError();
    chrome.test.assertEq(1, list.length);
    var browser = list[0];
    chrome.test.assertEq('Browser', browser.windowType);
    chrome.test.assertEq('Minimized', browser.stateType);
    chrome.autotestPrivate.setAppWindowState(
        browser.id, {eventType: 'WMEventNormal'}, true /* wait */,
        function(state) {
          chrome.test.assertNoLastError();
          chrome.test.assertEq('Normal', state);
          chrome.autotestPrivate.activateAppWindow(browser.id, callback);
        });
  });
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
          chrome.test.assertTrue(
              status.hasOwnProperty('isLockscreenWallpaperAnimating'));
          chrome.test.assertTrue(status.hasOwnProperty("isRegularUser"));
          chrome.test.assertTrue(status.hasOwnProperty("isGuest"));
          chrome.test.assertTrue(status.hasOwnProperty("isKiosk"));
          chrome.test.assertTrue(status.hasOwnProperty("email"));
          chrome.test.assertTrue(status.hasOwnProperty("displayEmail"));
          chrome.test.assertTrue(status.hasOwnProperty("userImage"));
          chrome.test.assertTrue(status.hasOwnProperty("hasValidOauth2Token"));
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
  function removeAllNotifications() {
    // Image data URL of a small red dot to use for the notification icon.
    var red_dot = 'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAUA' +
        'AAAFCAYAAACNbyblAAAAHElEQVQI12P4//8/w38GIAXDIBKE0DHxgljNBAAO' +
        '9TXL0Y4OHwAAAABJRU5ErkJggg=='
    var opts =
        {type: 'basic', title: 'test', message: 'test', iconUrl: red_dot};

    chrome.notifications.create('test', opts, function() {
      chrome.autotestPrivate.getVisibleNotifications(function(notifications) {
        chrome.test.assertTrue(notifications.length > 0);
        chrome.autotestPrivate.removeAllNotifications(function() {
          chrome.autotestPrivate.getVisibleNotifications(function(
              notifications) {
            chrome.test.assertEq(notifications.length, 0);
            chrome.test.succeed();
          });
        });
      });
    });
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
        // Revert to disable state as default in this test set.
        chrome.autotestPrivate.setPlayStoreEnabled(false, function() {});
        chrome.test.succeed();
      });
    });
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
  function waitForSystemWebAppsInstall() {
    chrome.autotestPrivate.waitForSystemWebAppsInstall(
      chrome.test.callbackPass()
    );
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
  function couldAllowCrostini() {
    chrome.autotestPrivate.couldAllowCrostini(chrome.test.callbackPass(
        result => {
          chrome.test.assertFalse(result);
        }));
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
  // The state dumped in Assistant API error message is AssistantAllowedState
  // defined in ash/public/cpp/assistant/assistant_state_base.h
  // 3 is DISALLOWED_BY_NONPRIMARY_USER from IsAssistantAllowedForProfile when
  // running under test without setting up a primary account.
  function setAssistantEnabled() {
    chrome.autotestPrivate.setAssistantEnabled(true, 1000 /* timeout_ms */,
        chrome.test.callbackFail(
            'Assistant not allowed - state: 3'));
  },
  function sendAssistantTextQuery() {
    chrome.autotestPrivate.sendAssistantTextQuery(
        'what time is it?' /* query */,
        1000 /* timeout_ms */,
        chrome.test.callbackFail(
            'Assistant not allowed - state: 3'));
  },
  function waitForAssistantQueryStatus() {
    chrome.autotestPrivate.waitForAssistantQueryStatus(10 /* timeout_s */,
        chrome.test.callbackFail(
            'Assistant not allowed - state: 3'));
  },
  // This test verifies the error message when trying to set Assistant-related
  // preferences without enabling Assistant service first.
  function setAllowedPref() {
    chrome.autotestPrivate.setAllowedPref(
        'settings.voice_interaction.hotword.enabled' /* pref_name */,
        true /* value */,
        chrome.test.callbackFail(
            'Unable to set the pref because Assistant has not been enabled.'));
    chrome.autotestPrivate.setAllowedPref(
        'settings.voice_interaction.context.enabled' /* pref_name */,
        true /* value */,
        chrome.test.callbackFail(
            'Unable to set the pref because Assistant has not been enabled.'));
    // Note that onboarding pref is a counter that can be set without
    // enabling Assistant at the same time.
    chrome.autotestPrivate.setAllowedPref(
        'ash.assistant.num_sessions_where_onboarding_shown' /* pref_name */,
        3 /* value */, chrome.test.callbackPass());
  },
  // This test verifies that getArcState returns provisioned False in case ARC
  // is not provisioned by default.
  function arcNotProvisioned() {
    chrome.autotestPrivate.getArcState(
        chrome.test.callbackPass(function(state) {
          chrome.test.assertFalse(state.provisioned);
          chrome.test.assertEq(0, state.preStartTime);
          chrome.test.assertEq(0, state.startTime);
        }));
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
        'Closed', chrome.test.callbackPass());
  },

  // This test verifies that api to wait for launcher state transition
  // works as expected
  function waitForLauncherStateFullscreen() {
    var toggleFullscreen = newAccelerator('search', true /* shift */);
    chrome.autotestPrivate.activateAccelerator(
        toggleFullscreen, function(success) {
          chrome.test.assertNoLastError();
          chrome.test.assertFalse(success);
          toggleFullscreen.pressed = false;
          chrome.autotestPrivate.activateAccelerator(
              toggleFullscreen, function(success) {
                chrome.test.assertNoLastError();
                chrome.test.assertTrue(success);
                chrome.autotestPrivate.waitForLauncherState(
                    'FullscreenAllApps', function() {
                      if (chrome.runtime.lastError) {
                        var errorMessage = chrome.runtime.lastError.message;
                        closeLauncher(chrome.test.callbackPass(function() {
                          chrome.test.assertEq(
                              'Not supported for bubble launcher',
                              errorMessage);
                        }));
                        return;
                      }
                      closeLauncher(chrome.test.callbackPass());
                    });
              });
        });
  },

  // This test verifies that api to wait for launcher state transition
  // to the same 'Closed' state when launcher is in closed state works.
  function waitForLauncherStateNoChangeAfterLauncherCreation() {
    chrome.autotestPrivate.waitForLauncherState(
        'Closed', chrome.test.callbackPass());
  },

  function waitForLauncherStateInTabletMode() {
    promisify(minimizeBrowserWindow)
        .then(function() {
          return promisify(chrome.autotestPrivate.setTabletModeEnabled, true);
        })
        .then(function() {
          return promisify(
              chrome.autotestPrivate.waitForLauncherState, 'FullscreenAllApps');
        })
        .then(function() {
          return promisify(chrome.autotestPrivate.setTabletModeEnabled, false);
        })
        .then(function() {
          return promisify(
              chrome.autotestPrivate.waitForLauncherState, 'Closed');
        })
        .then(function() {
          return promisify(unminimizeBrowserWindow);
        })
        .then(function() {
          chrome.test.succeed();
        })
        .catch(function(err) {
          chrome.test.fail(err);
        });
  },

  function collectThoughputTrackerData() {
    promisify(minimizeBrowserWindow)
        .then(function() {
          return promisify(
              chrome.autotestPrivate.startThroughputTrackerDataCollection);
        })
        .then(function() {
          // Triggers a tracked animation, e.g. enabling tablet mode to show
          // fullscreen launcher.
          return promisify(chrome.autotestPrivate.setTabletModeEnabled, true);
        })
        .then(function(enabled) {
          chrome.test.assertTrue(enabled);
          return promisify(
              chrome.autotestPrivate.waitForLauncherState, 'FullscreenAllApps');
        })
        .then(function() {
          return promisify(chrome.autotestPrivate.setTabletModeEnabled, false);
        })
        .then(function(enabled) {
          chrome.test.assertFalse(enabled);
          return promisify(
              chrome.autotestPrivate.waitForLauncherState, 'Closed');
        })
        .then(function() {
          return promisify(
              chrome.autotestPrivate.getThroughputTrackerData);
        })
        .then(function(data) {
          chrome.test.assertTrue(data.length > 0);
          return promisify(unminimizeBrowserWindow);
        })
        .then(function() {
          return promisify(
              chrome.autotestPrivate.stopThroughputTrackerDataCollection);
        })
        .then(function(data) {
          // `unminimizeBrowserWindow` might produce 0 frames on build bots
          // and end up not being captured in `data`.
          chrome.test.assertTrue(data.length >= 0);
          chrome.test.succeed();
        })
        .catch(function(err) {
          chrome.test.fail(err);
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
        chrome.test.assertEq(chromium.publisherId, "");
        chrome.test.assertEq(chromium.additionalSearchTerms, []);
        chrome.test.assertEq(chromium.readiness, 'Ready');
        chrome.test.assertEq(chromium.showInLauncher, true);
        chrome.test.assertEq(chromium.showInSearch, true);
        chrome.test.assertEq(chromium.type, 'Extension');
        chrome.test.assertEq(chromium.installSource, 'System');
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
      chrome.test.assertTrue(item.pinStateForcedByType);
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
      chrome.test.assertNe("-1", displayId);
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
      chrome.test.assertNe("-1", displayId);
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
      chrome.test.assertNe("-1", displayId);
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
      chrome.test.assertNe("-1", displayId);
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
        chrome.test.assertEq(null, window.fullRestoreWindowAppId);
        chrome.test.assertEq('mgndgikekgjfcpckkfioiadnlibdjbkf', window.appId);

        var change = new Object();
        change.eventType = 'WMEventFullscreen';
        chrome.autotestPrivate.setAppWindowState(
            window.id,
            change,
            true /* wait */,
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

                  // Revert window state back to normal for the next test.
                  var revert_change = new Object();
                  revert_change.eventType = 'WMEventNormal';
                  chrome.autotestPrivate.setAppWindowState(
                      window.id, revert_change, true /* wait */,
                      function(state) {
                        chrome.test.assertEq(state, 'Normal');
                        chrome.test.assertNoLastError();
                        chrome.test.succeed();
                      });
                });
              });
            });
      }
      chrome.test.assertNe(browserFrameIndex, -1);
    });
  },

  // Tests that setting the window state in tablet mode works.
  function setWindowStateInTabletMode() {
    chrome.autotestPrivate.setTabletModeEnabled(true, function(isEnabled) {
      chrome.test.assertTrue(isEnabled);

      chrome.autotestPrivate.getAppWindowList(function(list) {
        chrome.test.assertEq(1, list.length);
        var window = list[0];
        chrome.test.assertEq(window.stateType, 'Maximized');
        chrome.test.assertTrue(window.isVisible);
        chrome.test.assertTrue(window.isActive);

        var change = new Object();
        change.eventType = 'WMEventFullscreen';
        chrome.autotestPrivate.setAppWindowState(
            window.id, change, true /* wait */, function(state) {
              chrome.test.assertEq(state, 'Fullscreen');

              // Just send the rejectable request (normal state request in
              // tablet mode) but without waiting for the state change.
              const rejectable_change = {
                eventType: 'WMEventNormal'
              };
              chrome.autotestPrivate.setAppWindowState(
                  window.id, rejectable_change, false /* wait */,
                  function(state) {
                    chrome.autotestPrivate.setTabletModeEnabled(
                        false, function(isEnabled) {
                          chrome.test.assertFalse(isEnabled);

                          // Revert window state back to normal and exit tablet
                          // mode for the next test.
                          const revert_change = {
                            eventType: 'WMEventNormal'
                          };
                          chrome.autotestPrivate.setAppWindowState(
                              window.id, revert_change, true /* wait */,
                              function(state) {
                                chrome.test.assertEq(state, 'Normal');
                                chrome.test.assertNoLastError();
                                chrome.test.succeed();
                              });
                        });
                      });
            });
      });
    });
  },
  // This test verifies that api to activate accelrator works as expected.
  function acceleratorTest() {
    // Ash level accelerator.
    var newBrowser = newAccelerator('n', false /* shift */, true /* control */);
    chrome.autotestPrivate.activateAccelerator(newBrowser, function() {
      newBrowser.pressed = false;
      chrome.autotestPrivate.activateAccelerator(newBrowser, function() {
        chrome.autotestPrivate.getAppWindowList(function(list) {
          chrome.test.assertEq(2, list.length);
          var closeWindow =
              newAccelerator('w', false /* shift */, true /* control */);
          chrome.autotestPrivate.activateAccelerator(
              closeWindow, function(success) {
                chrome.test.assertTrue(success);
                closeWindow.pressed = false;
                chrome.autotestPrivate.activateAccelerator(
                    closeWindow, async function(success) {
                      chrome.test.assertNoLastError();
                      // Actual window close might happen sometime later after
                      // the accelerator. So keep trying until window count
                      // drops to 1.
                      await new Promise(resolve => {
                        function check() {
                          chrome.autotestPrivate.getAppWindowList(function(
                              list) {
                            chrome.test.assertNoLastError();

                            if (list.length == 1) {
                              resolve();
                              return;
                            }

                            window.setTimeout(check, 100);
                          });
                        };

                        check();
                      });
                      chrome.test.succeed();
                    });
              });
        });
      });
    });
  },
  // This test verifies that api to activate accelrator with number works as
  // expected.
  function acceleratorWithNumberTest() {
    // An ash accelerator with number to reset UI scale.
    var accelerator = newAccelerator('0', true /* shift */, true /* control */);
    chrome.autotestPrivate.activateAccelerator(
        accelerator, chrome.test.callbackPass((success) => {
          chrome.test.assertTrue(success);
          accelerator.pressed = false;
          chrome.autotestPrivate.activateAccelerator(
              accelerator, chrome.test.callbackPass());
        }));
  },
  function setMetricsEnabled() {
    chrome.autotestPrivate.setMetricsEnabled(true, chrome.test.callbackPass());
  },
  // This test verifies that the API to set window bounds works as expected.
  function setWindowBoundsTest() {
    chrome.autotestPrivate.getAppWindowList(function(list) {
      chrome.test.assertEq(1, list.length);
      var window = list[0];
      chrome.test.assertEq(window.stateType, 'Normal');
      chrome.test.assertTrue(window.isVisible);
      chrome.test.assertTrue(window.isActive);

      // Test changing the bounds.
      var newBounds = Object.assign({}, window.boundsInRoot);
      newBounds.width /= 2;
      newBounds.height /= 2;
      chrome.autotestPrivate.setWindowBounds(window.id, newBounds,
          window.displayId, function(result) {
            chrome.test.assertNoLastError();
            chrome.test.assertEq(result.bounds, newBounds);
            chrome.test.assertEq(result.displayId, window.displayId);
            // Reset bounds to original.
            chrome.autotestPrivate.setWindowBounds(window.id,
                window.boundsInRoot, window.displayId, function(result) {
                  chrome.test.assertNoLastError();
                  chrome.test.assertEq(result.bounds, window.boundsInRoot);
                  chrome.test.assertEq(result.displayId, window.displayId);

                  // Test calling setWindowBounds without changing the bounds
                  // succeeds.
                  chrome.autotestPrivate.setWindowBounds(window.id,
                      window.boundsInRoot, window.displayId, function(result) {
                        chrome.test.assertNoLastError();
                        chrome.test.assertEq(result.bounds,
                            window.boundsInRoot);
                        chrome.test.assertEq(result.displayId,
                            window.displayId);
                        // Test calling setWindowBounds with an invalid display
                        // fails.
                        chrome.autotestPrivate.setWindowBounds(window.id,
                            window.boundsInRoot, '-1', function(result) {
                              chrome.test.assertLastError(
                                  'Given display ID does not ' +
                                  'correspond to a valid display');
                              chrome.test.succeed();
                            });
                      });
                });
          });
    });
  },

  function startSmoothnessTracking() {
    chrome.autotestPrivate.startSmoothnessTracking(async function() {
      chrome.test.assertNoLastError();

      chrome.autotestPrivate.stopSmoothnessTracking(function(data) {
        chrome.test.assertNoLastError();
        chrome.test.assertTrue(data.hasOwnProperty('framesExpected') ||
                               data.hasOwnProperty('framesProduced') ||
                               data.hasOwnProperty('jankCount'));
        chrome.test.succeed();
      });
    });
  },
  function startSmoothnessTrackingExplicitThroughputInterval() {
    chrome.autotestPrivate.startSmoothnessTracking(10, async function() {
      chrome.test.assertNoLastError();

      // Let test run a bit to collect a few data points.
      // Minimizing/unminimizing to generate some screen changes.
      await sleep(100);
      await promisify(minimizeBrowserWindow);

      await sleep(200);
      await promisify(unminimizeBrowserWindow);

      chrome.autotestPrivate.stopSmoothnessTracking(function(data) {
        chrome.test.assertNoLastError();
        chrome.test.assertTrue(data.hasOwnProperty('throughput'));
        chrome.test.succeed();
      });
    });
  },
  function startSmoothnessTrackingExplicitDisplay() {
    const badDisplay = '-1';
    chrome.autotestPrivate.startSmoothnessTracking(badDisplay, function() {
      chrome.test.assertEq(chrome.runtime.lastError.message,
          'Invalid display_id; no root window found for the display id -1');
      chrome.system.display.getInfo(function(info) {
        var displayId = info[0].id;
        chrome.autotestPrivate.startSmoothnessTracking(displayId,
                                                       async function() {
          chrome.test.assertNoLastError();

          chrome.autotestPrivate.stopSmoothnessTracking(badDisplay,
                                                        function(data) {
            chrome.test.assertEq(chrome.runtime.lastError.message,
                'Smoothness is not tracked for display: -1');

            chrome.autotestPrivate.stopSmoothnessTracking(displayId,
                                                          function(data) {
              chrome.test.assertNoLastError();
              chrome.test.assertTrue(data.hasOwnProperty('framesExpected') ||
                                     data.hasOwnProperty('framesProduced') ||
                                     data.hasOwnProperty('jankCount'));
              chrome.test.succeed();
            });
          });
        });
      });
    });
  },
  function stopSmoothnessTrackingMultiple() {
    chrome.autotestPrivate.startSmoothnessTracking(async function() {
      chrome.test.assertNoLastError();

      // A few racing stopSmoothnessTracking calls.
      const count = 3;
      let promises = [];
      for (let i = 0; i < count; ++i)
        promises.push(promisify(chrome.autotestPrivate.stopSmoothnessTracking));

      // Only one should succeed and no crashes/DCHECKs.
      let success = 0;
      for (let i = 0; i < count; ++i) {
        try {
          await promises[i];
          ++success;
        } catch(error) {}
      }
      chrome.test.assertEq(success, 1);
      chrome.test.succeed();
    });
  },

  function getDisplaySmoothness() {
    chrome.autotestPrivate.getDisplaySmoothness(function(smoothness) {
      chrome.test.assertNoLastError();

      chrome.test.assertTrue(smoothness >= 0);
      chrome.test.succeed();
    });
  },

  function setAndGetClipboardTextData() {
    const textData = 'foo bar';
    chrome.autotestPrivate.getClipboardTextData(function(beforeData) {
      chrome.test.assertNe(beforeData, textData);
      chrome.autotestPrivate.setClipboardTextData(textData, function() {
        chrome.autotestPrivate.getClipboardTextData(function(afterData) {
          chrome.test.assertEq(afterData, textData);
          chrome.test.succeed();
        });
      });
    });
  },

  function setClipboardTextDataTwice() {
    const textData = 'twice clipboard data';
    chrome.autotestPrivate.setClipboardTextData(textData, function() {
      chrome.autotestPrivate.setClipboardTextData(textData, function() {
        chrome.autotestPrivate.getClipboardTextData(function(data) {
          chrome.test.assertEq(data, textData);
          chrome.test.succeed();
        });
      });
    });
  },

  function collectLoginEventRecorderData() {
    chrome.autotestPrivate.startLoginEventRecorderDataCollection(function() {
      chrome.test.assertNoLastError();

      // Add new event to the login events log and check result.
      chrome.autotestPrivate.addLoginEventForTesting(function() {
        chrome.test.assertNoLastError();
        chrome.autotestPrivate.getLoginEventRecorderLoginEvents(
            function(data){
          chrome.test.assertNoLastError();
          chrome.test.assertTrue(data.length >= 0);
          chrome.test.succeed();
        });
      });
    });
  },

  function collectFrameCountingData() {
    let extraWindow;
    promisify(
        chrome.autotestPrivate.startFrameCounting, /*bucketSizeInSeconds=*/1)
        .then(function() {
          // Create a browser window after start api call.
          return new Promise(resolve => {
            extraWindow = window.open("about:blank");
            resolve();
          });
        })
        .then(function() {
          // Minimize/restore to trigger screen updates.
          return promisify(minimizeBrowserWindow);
        })
        .then(function() {
          return promisify(unminimizeBrowserWindow);
        })
        .then(function() {
          return promisify(
              chrome.autotestPrivate.stopFrameCounting);
        })
        .then(function(data) {
          extraWindow.close();
          chrome.test.assertTrue(data.length >= 0);
          chrome.test.succeed();
        })
        .catch(function(err) {
          if (extraWindow)
            extraWindow.close();
          chrome.test.fail(err);
        });
  },

  function stopFrameCountingWithoutStart() {
    // Expects the stop call to fail when not paired with a start call.
    chrome.autotestPrivate.stopFrameCounting(
        chrome.test.callbackFail('No frame counting data'));
  },

  async function startOverdrawTracking() {
    let browserWindow;
    try {
      // Wait so that gpu process can fully initialize.
      await sleep(500);
      await promisify(
          chrome.autotestPrivate.startOverdrawTracking,
          /*bucketSizeInSeconds=*/ 1);

      // Perform a UI action to generate compositor frames so that overdraw
      // of those frames can be tracked.
      await new Promise(resolve => {
        browserWindow = window.open('about:blank');
        resolve();
      });

      const data = await promisify(chrome.autotestPrivate.stopOverdrawTracking);

      chrome.test.assertTrue(!!data);
      chrome.test.assertTrue(data.averageOverdraws.length > 0);
      browserWindow.close();

      chrome.test.succeed();
    } catch (error) {
      if (browserWindow) {
        browserWindow.close();
      }
      chrome.test.fail();
    }
  },

  async function collectOverdrawDataWithExplicitDisplayId() {
    const displaysInfo = await promisify(chrome.system.display.getInfo);
    const displayId = displaysInfo[0].id;

    let browserWindow;

    try {
      // Wait so that gpu process can fully initialize.
      await sleep(500);
      await promisify(
          chrome.autotestPrivate.startOverdrawTracking,
          /*bucketSizeInSeconds=*/ 1),
          displayId;

      // Perform a UI action to generate compositor frames so that overdraw
      // of those frames can be tracked.
      await new Promise(resolve => {
        browserWindow = window.open('about:blank');
        resolve();
      });

      const data = await promisify(chrome.autotestPrivate.stopOverdrawTracking);

      chrome.test.assertTrue(!!data);
      chrome.test.assertTrue(data.averageOverdraws.length > 0);
      browserWindow.close();

      chrome.test.succeed();
    } catch (error) {
      if (browserWindow) {
        browserWindow.close();
      }
      chrome.test.fail();
    }
  },

  async function noOverdrawDataCollectedBetweenStartAndStopCalls() {
    try {
      // Wait so that gpu process can fully initialize.
      await sleep(500);
      await promisify(
          chrome.autotestPrivate.startOverdrawTracking,
          /*bucketSizeInSeconds=*/ 1);

      // No data since no compositor frame was submitted to viz in between start
      // and stop calls. (No overdraw data is treated as an error)
      await promisify(chrome.autotestPrivate.stopOverdrawTracking);

      chrome.test.fail();
    } catch (error) {
      chrome.test.assertEq(
          error.message,
          'No overdraw data; maybe forgot to call startOverdrawTracking or ' +
              'no UI changes between start and stop calls');
      chrome.test.succeed();
    }
  },

  async function stopCollectingOverdrawDataWithoutStart() {
    try {
      // Wait so that gpu process can fully initialize.
      await sleep(500);
      await promisify(chrome.autotestPrivate.stopOverdrawTracking);
      chrome.test.fail();
    } catch (error) {
      chrome.test.assertEq(
          error.message,
          'No overdraw data; maybe forgot to call startOverdrawTracking or ' +
              'no UI changes between start and stop calls');
      chrome.test.succeed();
    }
  },

  async function startCollectingOverdrawDataForInvalidDisplay() {
    const badDisplayId = '-1';
    try {
      await promisify(
          chrome.autotestPrivate.startOverdrawTracking,
          /*bucketSizeInSeconds=*/ 1, badDisplayId);
      chrome.test.fail();
    } catch (error) {
      chrome.test.assertEq(
          error.message,
          'Invalid displayId; no display found for the display id -1');
      chrome.test.succeed();
    }
  },

  async function stopCollectingOverdrawDataForInvalidDisplay() {
    const badDisplayId = '-1';
    try {
      await promisify(
          chrome.autotestPrivate.stopOverdrawTracking, badDisplayId);
      chrome.test.fail();
    } catch (error) {
      chrome.test.assertEq(
          error.message,
          'Invalid displayId; no display found for the display id -1');
      chrome.test.succeed();
    }
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
    chrome.autotestPrivate.getArcState(
        chrome.test.callbackPass(function(state) {
          chrome.test.assertTrue(state.provisioned);
          chrome.test.assertTrue(state.preStartTime > 0);
          chrome.test.assertTrue(state.startTime > 0);
          chrome.test.assertTrue(state.startTime >= state.preStartTime);
          chrome.test.assertTrue((new Date()).getTime() >= state.startTime);
        }));
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
          chrome.test.assertEq(false, packageInfo.vpnProvider);
          chrome.test.succeed();
        }));
  }
];

var arcProcessTests = [
  async function requestLowMemoryKillCounts() {
    const counts = await promisify(chrome.autotestPrivate.getArcAppKills);
    chrome.test.assertEq(counts.oom, 1);
    chrome.test.assertEq(counts.lmkdForeground, 2);
    chrome.test.assertEq(counts.lmkdPerceptible, 3);
    chrome.test.assertEq(counts.lmkdCached, 4);
    chrome.test.assertEq(counts.pressureForeground, 5);
    chrome.test.assertEq(counts.pressurePerceptible, 6);
    chrome.test.assertEq(counts.pressureCached, 7);
    chrome.test.succeed();
  }
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
            "extensionPolicies":{},
            "loginScreenExtensionPolicies":{}
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

var remoteCommandsTests = [
  function refreshRemoteCommands() {
    chrome.autotestPrivate.refreshRemoteCommands(
      chrome.test.callbackPass(function () {
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

var splitviewPrimarySnappedTests = [
  function getSplitViewControllerStatePrimarySnapped() {
    chrome.autotestPrivate.getAppWindowList(
        chrome.test.callbackPass(function(list) {
          var found = false;
          list.forEach(window => {
            if (window.stateType == 'PrimarySnapped')
              found = true;
          });
          chrome.test.assertTrue(found);
        }));
  }
];

var scrollableShelfTests = [
  function fetchScrollableShelfInfoWithoutScroll() {
    chrome.autotestPrivate.getScrollableShelfInfoForState(
        {}, chrome.test.callbackPass(info => {
          chrome.test.assertEq(0, info.mainAxisOffset);
          chrome.test.assertEq(0, info.rightArrowBounds.width);
          chrome.test.assertFalse(info.iconsUnderAnimation);
          chrome.test.assertFalse(info.hasOwnProperty('targetMainAxisOffset'));
        }));
  },

  function fetchScrolableShelfInfoWithScroll() {
    chrome.autotestPrivate.getScrollableShelfInfoForState(
        {'scrollDistance': 10}, chrome.test.callbackPass(info => {
          chrome.test.assertEq(0, info.mainAxisOffset);
          chrome.test.assertEq(0, info.rightArrowBounds.width);
          chrome.test.assertFalse(info.iconsUnderAnimation);
          chrome.test.assertTrue(info.hasOwnProperty('targetMainAxisOffset'));
        }));
  },

  function pinShelfIcon() {
    chrome.autotestPrivate.getAllInstalledApps(
        chrome.test.callbackPass(apps => {
          apps.forEach(app => {
            chrome.autotestPrivate.pinShelfIcon(
                app.appId, chrome.test.callbackPass());
          });
        }));
  },

  async function unpinChromeBrowser() {
    try {
      await promisify(
          chrome.autotestPrivate.setShelfIconPin,
          [{appId: 'mgndgikekgjfcpckkfioiadnlibdjbkf', pinned: false}]);
      chrome.test.fail();
    } catch (error) {
      // Unpinning an app which is unpinnable (such as the browser) should throw
      // an error.
      chrome.test.assertTrue(error.message.includes(
          'Unable to update pin state: mgndgikekgjfcpckkfioiadnlibdjbkf'));
      chrome.test.succeed();
    }
  },

  async function pinInstalledApps() {
    var installedApps =
        await promisify(chrome.autotestPrivate.getAllInstalledApps);
    var updateParams = [];
    installedApps.forEach(app => {
      obj = {appId: app.appId, pinned: true};
      updateParams.push(obj);
    });

    var pinResults =
        await promisify(chrome.autotestPrivate.setShelfIconPin, updateParams);
    chrome.test.assertEq([], pinResults);
    chrome.test.succeed();
  },

  async function pinThenUnpinFileApp() {
    // Pin the File app.
    var fileID = 'unique-file-id-123'
    var pinResults = await promisify(
        chrome.autotestPrivate.setShelfIconPin,
        [{appId: fileID, pinned: true}]);

    chrome.test.assertEq([fileID], pinResults);

    // Unpin the File app.
    var unpinResults = await promisify(
        chrome.autotestPrivate.setShelfIconPin,
        [{appId: fileID, pinned: false}]);

    chrome.test.assertEq([fileID], unpinResults);

    // Unpin the File app again.
    unpinResults = await promisify(
        chrome.autotestPrivate.setShelfIconPin,
        [{appId: fileID, pinned: false}]);

    // Because the File app has been unpinned, there is no update in pin state.
    chrome.test.assertEq([], unpinResults);

    chrome.test.succeed();
  }
];

var shelfTests = [function fetchShelfUIInfo() {
  chrome.autotestPrivate.setTabletModeEnabled(
      false, chrome.test.callbackPass(isEnabled => {
        chrome.test.assertFalse(isEnabled);
        chrome.autotestPrivate.getShelfUIInfoForState(
            {}, chrome.test.callbackPass(info => {
              chrome.test.assertEq('ShownClamShell', info.hotseatInfo.state);
            }));
      }));
}];

var isFeatureEnabledTests = [
  function getEnabledFeature() {
    chrome.autotestPrivate.isFeatureEnabled("EnabledFeatureForTest",
      chrome.test.callbackPass(enabled => {
        chrome.test.assertTrue(enabled);
      }));
  },
  function getDisabledFeature() {
    chrome.autotestPrivate.isFeatureEnabled("DisabledFeatureForTest",
      chrome.test.callbackPass(enabled => {
        chrome.test.assertFalse(enabled);
      }));
  },
  function getUnknownFeature() {
    chrome.autotestPrivate.isFeatureEnabled("UnknownFeature",
      chrome.test.callbackFail(
        "feature UnknownFeature is not on allowlist, see " +
        "AutotestPrivateIsFeatureEnabledFunction::Run() to update the list"));
  }
];

var launcherSearchBoxStateTests = [ function verifyGhostText(){
  chrome.autotestPrivate.getLauncherSearchBoxState(
      chrome.test.callbackPass(info => {
        chrome.test.assertEq('youtube - Websites', info.ghostText);
      }));
}];

var holdingSpaceTests = [
  function resetHoldingSpace(options) {
    // State after this call is checked in C++ test code.
    chrome.autotestPrivate.resetHoldingSpace(options,
      chrome.test.callbackPass());
  },
];

var isFieldTrialActiveTests = [
  function getActiveTrialActiveGroup() {
    chrome.autotestPrivate.isFieldTrialActive(
        'ActiveTrialForTest', 'GroupForTest',
        chrome.test.callbackPass(enabled => {
          chrome.test.assertTrue(enabled);
        }));
  },
  function getActiveTrialInactiveGroup() {
    chrome.autotestPrivate.isFieldTrialActive(
        'ActiveTrialForTest', 'WrongGroupForTest',
        chrome.test.callbackPass(enabled => {
          chrome.test.assertFalse(enabled);
        }));
  },
  function getInactiveTrial() {
    chrome.autotestPrivate.isFieldTrialActive(
        'InactiveTrialForTest', 'GroupForTest',
        chrome.test.callbackPass(enabled => {
          chrome.test.assertFalse(enabled);
        }));
  }
];

var clearAllowedPrefTests = [
  function clearAllowedPrefs(pref_name) {
    chrome.autotestPrivate.clearAllowedPref(pref_name,
        chrome.test.callbackPass());
  }
];

var setDeviceLanguage = [
  function setDeviceLanguage(locale) {
    chrome.autotestPrivate.setDeviceLanguage(locale,
        chrome.test.callbackPass());
  }
];

var getDeviceEventLog = [
  function getDeviceEventLogSingle() {
    chrome.autotestPrivate.getDeviceEventLog('printer',
      chrome.test.callbackPass(logs => {
        chrome.test.assertTrue(logs.includes('PrinterTestLog'));
        chrome.test.assertFalse(logs.includes('NetworkTestLog'));
        chrome.test.assertFalse(logs.includes('USBTestLog'));
      }));
  },
  function getDeviceEventLogMultiple() {
    chrome.autotestPrivate.getDeviceEventLog('printer,network',
      chrome.test.callbackPass(logs => {
        chrome.test.assertTrue(logs.includes('PrinterTestLog'));
        chrome.test.assertTrue(logs.includes('NetworkTestLog'));
        chrome.test.assertFalse(logs.includes('USBTestLog'));
      }));
  },
  function getDeviceEventLogAll() {
    chrome.autotestPrivate.getDeviceEventLog('',
      chrome.test.callbackPass(logs => {
        chrome.test.assertTrue(logs.includes('PrinterTestLog'));
        chrome.test.assertTrue(logs.includes('NetworkTestLog'));
        chrome.test.assertTrue(logs.includes('USBTestLog'));
      }));
  }
];

// Tests that requires a concrete system web app installation.
var systemWebAppsTests = [
  function getRegisteredSystemWebApps() {
    chrome.autotestPrivate.getRegisteredSystemWebApps(
      chrome.test.callbackPass(apps => {
        chrome.test.assertEq(1, apps.length)
        chrome.test.assertEq('OSSettings', apps[0].internalName);
        chrome.test.assertEq('chrome://test-system-app/', apps[0].url);
        chrome.test.assertEq('chrome://test-system-app/pwa.html',
            apps[0].startUrl);
        chrome.test.assertEq('Test System App', apps[0].name);
      })
    );
  },
  async function isSystemWebAppOpen() {
    const waitForSystemWebAppsInstall = (...args) =>
        promisify(chrome.autotestPrivate.waitForSystemWebAppsInstall, ...args);
    const isSystemWebAppOpen = (...args) =>
        promisify(chrome.autotestPrivate.isSystemWebAppOpen, ...args);
    const launchSystemWebApp = (...args) =>
        promisify(chrome.autotestPrivate.launchSystemWebApp, ...args);

    await waitForSystemWebAppsInstall();

    // Checking for an invalid app should fail.
    let did_error = false;
    await isSystemWebAppOpen('').catch(() => did_error = true);
    chrome.test.assertTrue(did_error, 'Checking an invalid app should error');
    chrome.test.assertLastError('No system web app is found by given app id.');

    // App isn't opened at the start.
    chrome.test.assertFalse(
        await isSystemWebAppOpen('maphiehpiinjgiaepbljmopkodkadcbh'),
        'App shouldn\'t be opened before launchSystemWebApp');

    // Launch an app.
    await launchSystemWebApp('OSSettings', 'chrome://test-system-app/');

    // App launch might be queued and processed later. We don't have a method to
    // wait for launch completion, so we poll instead. If this test times out,
    // most likely something is wrong with system web app launch logic.
    while (!await isSystemWebAppOpen('maphiehpiinjgiaepbljmopkodkadcbh')) {
      await sleep(100);
    }

    chrome.test.succeed();
  },
]

    var lacrosEnabledTests = [
      function checkLacrosInfoFields() {
        chrome.autotestPrivate.getLacrosInfo(
            chrome.test.callbackPass(function(lacrosInfo) {
              chrome.test.assertEq(typeof lacrosInfo, 'object');
              chrome.test.assertTrue(lacrosInfo.hasOwnProperty('state'));
              chrome.test.assertTrue(lacrosInfo.hasOwnProperty('isKeepAlive'));
              chrome.test.assertTrue(lacrosInfo.hasOwnProperty('lacrosPath'));
              chrome.test.assertTrue(lacrosInfo.hasOwnProperty('mode'));
            }));
      },
      function checkLacrosInfoFieldValue() {
        chrome.autotestPrivate.getLacrosInfo(
            chrome.test.callbackPass(function(lacrosInfo) {
              chrome.test.assertEq('Unavailable', lacrosInfo['state']);
              chrome.test.assertTrue(lacrosInfo['isKeepAlive']);
              chrome.test.assertEq('', lacrosInfo['lacrosPath']);
              chrome.test.assertEq('Only', lacrosInfo['mode']);
            }));
      },
    ]

    var test_suites = {
      'default': defaultTests,
      'arcEnabled': arcEnabledTests,
      'arcProcess': arcProcessTests,
      'enterprisePolicies': policyTests,
      'remoteCommands': remoteCommandsTests,
      'arcPerformanceTracing': arcPerformanceTracingTests,
      'overviewDefault': overviewTests,
      'overviewDrag': overviewDragTests,
      'splitviewPrimarySnapped': splitviewPrimarySnappedTests,
      'scrollableShelf': scrollableShelfTests,
      'shelf': shelfTests,
      'isFeatureEnabled': isFeatureEnabledTests,
      'holdingSpace': holdingSpaceTests,
      'systemWebApps': systemWebAppsTests,
      'lacrosEnabled': lacrosEnabledTests,
      'launcherSearchBoxState': launcherSearchBoxStateTests,
      'isFieldTrialActive': isFieldTrialActiveTests,
      'clearAllowedPref': clearAllowedPrefTests,
      'setDeviceLanguage': setDeviceLanguage,
      'getDeviceEventLog': getDeviceEventLog
    };

chrome.test.getConfig(function(config) {
  var customArg = JSON.parse(config.customArg);
  // In the customArg object, we expect the name of the test suite at the
  // 'testSuite' key, and the arguments to be passed to the test functions as an
  // array at the 'args' key.
  var [suite_name, args] = [customArg['testSuite'], customArg['args']];

  chrome.test.assertTrue(Array.isArray(args));

  if (suite_name in test_suites) {
    var suite = test_suites[suite_name].map(f => f.bind({}, ...args));
    chrome.test.runTests(suite);
  } else {
    chrome.test.fail('Invalid test suite');
  }
});
