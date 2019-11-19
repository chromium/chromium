// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN('#include "chrome/browser/ui/browser.h"');
GEN('#include "chrome/browser/ui/browser_commands.h"');
GEN('#include "chrome/browser/ui/exclusive_access/' +
    'fullscreen_controller_test.h"');

/**
 * Fixture for ChromeOs WebUI OOBE testing.
 *
 * There's one test for each page in the Chrome OS Out-of-box-experience
 * (OOBE), so that an accessibility audit can be run automatically on
 * each one. This will alert a developer immediately if they accidentally
 * forget to label a control, or if a focusable control ends up
 * off-screen without being disabled, for example.
 * @constructor
 */
function OobeWebUITest() {}

OobeWebUITest.prototype = {
  __proto__: testing.Test.prototype,

  /** @override */
  browsePreload: 'chrome://oobe/oobe',

  /** @override */
  runAccessibilityChecks: true,

  /** @override */
  accessibilityIssuesAreErrors: true,

  /** @override */
  testGenPreamble: function() {
    // OobeWebUI should run in fullscreen.
    GEN('FullscreenNotificationObserver fullscreen_observer(browser());');
    GEN('chrome::ToggleFullscreenMode(browser());');
    GEN('fullscreen_observer.Wait();');
  },

  /** @override */
  setUp: function() {
    testing.Test.prototype.setUp.call(this);

    /* False positives. http://crbug.com/501489 */
    this.accessibilityAuditConfig.ignoreSelectors('badAriaAttributeValue',
                                                  '#cancelConfirmDlg');
    this.accessibilityAuditConfig.ignoreSelectors(
        'nonExistentAriaLabelledbyElement',
        '#cancelConfirmDlg');

    /**
     * False positive AX_FOCUS_01.
     * https://github.com/GoogleChrome/accessibility-developer-tools/issues/171
     */
    this.accessibilityAuditConfig.ignoreSelectors(
        'focusableElementNotVisibleAndNotAriaHidden',
        '#iconButton');

    // Enable when failure is resolved.
    // AX_ARIA_02: http://crbug.com/560932
    this.accessibilityAuditConfig.ignoreSelectors(
      'nonExistentAriaRelatedElement',
      '#cancelConfirmDlg');

    var requiredOwnedAriaRoleMissingSelectors = [
      '#supervised-user-creation-image-grid',
      'body > .decorated',
    ];

    // Enable when failure is resolved.
    // AX_ARIA_08: http://crbug.com/560924
    this.accessibilityAuditConfig.ignoreSelectors(
      'requiredOwnedAriaRoleMissing',
      requiredOwnedAriaRoleMissingSelectors);

    // Polymer.PaperButtonBehavior.ariaActiveAttribute starts with aria- but is
    // not an ARIA attribute. Ignore for paper-icon-button.
    this.accessibilityAuditConfig.ignoreSelectors(
      'badAriaAttribute',
      'paper-icon-button[aria-active-attribute]');

    var badAriaAttributeSelectors = [
      '#forgotPasswordDlg',
      '#cancelConfirmDlg',
    ];

    // Enable when failure is resolved.
    // AX_ARIA_11: http://crbug.com/560935
    this.accessibilityAuditConfig.ignoreSelectors(
      'badAriaAttribute',
      badAriaAttributeSelectors);

    var controlsWithoutLabelSelectors = [
      '#supervised-user-creation-managers-pane',
      '#supervised-user-creation-name',
      '#supervised-user-creation-password',
      '#supervised-user-creation-password-confirm',
    ];

    // Enable when failure is resolved.
    // AX_TEXT_01: http://crbug.com/560939
    this.accessibilityAuditConfig.ignoreSelectors(
      'controlsWithoutLabel',
      controlsWithoutLabelSelectors);
  },
};

function createOobeWebUITestSupervisedManagerData() {
  return { 'managers':
           [
             { 'username' : 'user@invalid.domain',
               'displayName' : 'John Doe',
               'emailAddress' : 'user@invalid.domain'
             },
             { 'username' : 'other@invalid.domain',
               'displayName' : 'Joanna Doe',
               'emailAddress' : 'other@invalid.domain'
             }
           ]
         };
}

// Fails on chromeos, http://crbug.com/665315
TEST_F('OobeWebUITest', 'DISABLED_EmptyOobe', function() {
});

// Fails on chromeos, http://crbug.com/665315
TEST_F('OobeWebUITest', 'DISABLED_OobeConnect', function() {
  Oobe.getInstance().showScreen({'id':'connect'});
});

// Fails on chromeos, http://crbug.com/665315
TEST_F('OobeWebUITest', 'DISABLED_OobeEula', function() {
  Oobe.getInstance().showScreen({'id':'eula'});
});

// Fails on chromeos, http://crbug.com/665315
TEST_F('OobeWebUITest', 'DISABLED_OobeUpdate', function() {
  Oobe.getInstance().showScreen({'id':'update'});
});

// Fails on chromeos, http://crbug.com/665315
TEST_F('OobeWebUITest', 'DISABLED_OobeGaiaSignIn', function() {
  Oobe.getInstance().showScreen({'id':'gaia-signin'});
});

// Fails on chromeos, http://crbug.com/665315
TEST_F('OobeWebUITest', 'DISABLED_OobeSupervisedUsers', function() {
  Oobe.getInstance().showScreen(
      {'id'   : 'supervised-user-creation',
       'data' : createOobeWebUITestSupervisedManagerData()});
});

// Fails on chromeos, http://crbug.com/665315
TEST_F('OobeWebUITest', 'DISABLED_OobeSupervisedUsers2', function() {
  Oobe.getInstance().showScreen(
      {'id'   : 'supervised-user-creation',
       'data' : createOobeWebUITestSupervisedManagerData()});
  $('supervised-user-creation').setVisiblePage_('manager');
});

// Fails on chromeos, http://crbug.com/665315
TEST_F('OobeWebUITest', 'DISABLED_OobeSupervisedUsers3', function() {
  Oobe.getInstance().showScreen(
      {'id'   : 'supervised-user-creation',
       'data' : createOobeWebUITestSupervisedManagerData()});
  $('supervised-user-creation').setDefaultImages(
      {'first' : 0,
       'images' : [{'url': 'chrome://nothing/', 'title': 'None'},
                   {'url': 'chrome://nothing/', 'title': 'None'}]});
  $('supervised-user-creation').setVisiblePage_('username');
});

// TODO: this either needs a WebUILoginDisplay instance or some
// other way to initialize the appropriate C++ handlers.
TEST_F('OobeWebUITest', 'DISABLED_OobeUserImage', function() {
  Oobe.getInstance().showScreen({'id':'user-image'});
});

// TODO: figure out what state to mock in order for this
// screen to show up.
TEST_F('OobeWebUITest', 'DISABLED_OobeAccountPicker', function() {
  Oobe.getInstance().showScreen({'id':'account-picker'});
});

// Fails on chromeos, http://crbug.com/665315
TEST_F('OobeWebUITest', 'DISABLED_HIDDetectionScreenTest', function() {
  function getPincodeSymbol(i) {
    return $('hid-keyboard-pincode-sym-' + (i + 1));
  }

  function getDisplayedPincode() {
    var pincode = '';
    for (var i = 0; i < 6; ++i)
      pincode += getPincodeSymbol(i).textContent;
    return pincode;
  }

  login.HIDDetectionScreen.contextChanged({
    'keyboard-state': 'searching',
    'mouse-state': 'searching'
  });
  Oobe.showScreen({'id': 'hid-detection'});
  expectTrue($('hid-keyboard-pincode').hidden);

  login.HIDDetectionScreen.contextChanged({
    'keyboard-state': 'pairing',
    'keyboard-pincode': '013188'
  });
  expectFalse($('hid-keyboard-pincode').hidden);
  expectEquals('013188', getDisplayedPincode());

  login.HIDDetectionScreen.contextChanged({
    'num-keys-entered-expected': true,
    'num-keys-entered-pincode': 3
  });
  expectFalse($('hid-keyboard-pincode').hidden);
  expectEquals('013188', getDisplayedPincode());
  [
    { 'key-typed': true },
    { 'key-typed': true },
    { 'key-typed': true },
    { 'key-next': true },
    { 'key-untyped': true },
    { 'key-untyped': true },
    { 'key-untyped': true }  // Enter key symbol.
  ].forEach(function(expectedClasses, i) {
    var symbol = getPincodeSymbol(i);
    ['key-typed', 'key-untyped', 'key-next'].forEach(function(className) {
      expectEquals(!!expectedClasses[className],
                   symbol.classList.contains(className));
    });
  });

  login.HIDDetectionScreen.contextChanged({
    'keyboard-state': 'connected'
  });
  expectTrue($('hid-keyboard-pincode').hidden);
});
