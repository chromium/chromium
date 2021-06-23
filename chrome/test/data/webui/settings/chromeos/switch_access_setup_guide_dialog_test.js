// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/lazy_load.js';

// #import {assertTrue, assertFalse} from '../../chai_assert.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// clang-format on

suite('SwitchAccessSetupGuideDialogTest', function() {
  /** @type {SettingsSwitchAccessSetupGuideDialog} */
  let dialog;

  setup(function() {
    dialog =
        document.createElement('settings-switch-access-setup-guide-dialog');
    dialog.prefs = {
      settings: {
        'a11y': {
          'switch_access': {
            'auto_scan': {
              'enabled': {
                value: false,
              },
              'speed_ms': {
                value: 1000,
              },
            },
          },
        },
      },
    };
    document.body.appendChild(dialog);
    Polymer.dom.flush();
  });

  test('Exit button closes dialog', function() {
    assertTrue(dialog.$.switchAccessSetupGuideDialog.open);

    const exitButton = dialog.$.exit;
    assertTrue(!!exitButton);

    exitButton.click();
    assertFalse(dialog.$.switchAccessSetupGuideDialog.open);
  });

  test('Navigation between dialog pages', function() {
    assertTrue(dialog.$.switchAccessSetupGuideDialog.open);
    assertEquals(/*Intro=*/0, dialog.currentPageId_);

    const nextButton = dialog.$.next;
    assertTrue(!!nextButton);
    nextButton.click();

    assertEquals(/*Assign select=*/1, dialog.currentPageId_);
    nextButton.click();

    assertEquals(/*Auto-scan enabled=*/2, dialog.currentPageId_);
    nextButton.click();

    assertEquals(/*Choose switch count=*/3, dialog.currentPageId_);
    nextButton.click();

    assertEquals(/*Auto-scan speed=*/4, dialog.currentPageId_);
    nextButton.click();

    assertEquals(/*Closing=*/8, dialog.currentPageId_);

    const previousButton = dialog.$.previous;
    assertTrue(!!previousButton);
    previousButton.click();

    assertEquals(/*Auto-scan speed=*/4, dialog.currentPageId_);
    previousButton.click();

    assertEquals(/*Choose switch count=*/3, dialog.currentPageId_);
    previousButton.click();

    assertEquals(/*Auto-scan enabled=*/2, dialog.currentPageId_);
    previousButton.click();

    assertEquals(/*Assign select=*/1, dialog.currentPageId_);
    previousButton.click();

    assertEquals(/*Intro=*/0, dialog.currentPageId_);

    // Check that the "Start Over" button takes us from the closing page back to
    // the intro.
    dialog.currentPageId_ = /*Closing=*/8;

    const startOverButton = dialog['$']['start-over'];
    assertTrue(!!startOverButton);
    startOverButton.click();

    assertEquals(/*Intro=*/0, dialog.currentPageId_);
  });

  test('Page contents are hidden and shown as expected', function() {
    assertTrue(dialog.$.switchAccessSetupGuideDialog.open);

    dialog.loadPage_(/*Intro=*/0);

    // Verify the contents of the Intro page.
    assertFalse(dialog['$']['intro']['hidden']);

    dialog.loadPage_(/*Assign select=*/1);

    // Verify the contents of the assign select page.
    assertTrue(dialog['$']['intro']['hidden']);
    assertFalse(dialog['$']['assign-select']['hidden']);

    dialog.loadPage_(/*Auto-scan enabled=*/2);

    // Verify the contents of the auto-scan enabled page.
    assertTrue(dialog['$']['assign-select']['hidden']);
    assertFalse(dialog['$']['auto-scan-enabled']['hidden']);

    dialog.loadPage_(/*Choose switch count=*/3);

    // Verify the contents of the choose switch count page.
    assertTrue(dialog['$']['auto-scan-enabled']['hidden']);
    assertFalse(dialog['$']['choose-switch-count']['hidden']);

    dialog.loadPage_(/*Auto-scan speed=*/4);

    // Verify the contents of the auto-scan speed page.
    assertTrue(dialog['$']['choose-switch-count']['hidden']);
    assertFalse(dialog['$']['auto-scan-speed']['hidden']);

    dialog.loadPage_(/*Closing=*/8);

    // Verify the contents of the closing page.
    assertTrue(dialog['$']['auto-scan-speed']['hidden']);
    assertFalse(dialog['$']['closing']['hidden']);
  });

  test('Auto-scan enabled and disabled correctly', function() {
    let setPrefData = [];
    // Mock this API to confirm it's getting called, and with the right values.
    chrome.settingsPrivate.setPref = function(key, value) {
      setPrefData.push({key, value});
    };

    assertTrue(dialog.$.switchAccessSetupGuideDialog.open);
    assertFalse(
        dialog.prefs.settings.a11y.switch_access.auto_scan.enabled.value);

    // Mock that we are on the page before auto scan is enabled.
    dialog.currentPageId_ = /*Assign select=*/1 ;

    // Moving forward should enable auto-scan.
    dialog.onNextClick_();
    assertEquals(/*Auto-scan enabled=*/2, dialog.currentPageId_);

    // Check that the settings API was called with the correct parameters.
    assertEquals(/*Assign select=*/1, setPrefData.length);
    assertEquals(
        'settings.a11y.switch_access.auto_scan.enabled', setPrefData[0].key);
    assertEquals(true, setPrefData[0].value);

    // Moving backward should disable auto-scan.
    dialog.onPreviousClick_();
    assertNotEquals(dialog.currentPageId_, /*Auto-scan enabled=*/2);

    assertEquals(2, setPrefData.length);
    assertEquals(
        'settings.a11y.switch_access.auto_scan.enabled', setPrefData[1].key);
    assertEquals(false, setPrefData[1].value);

    // Check that navigating backwards does not disable auto-scan if it was
    // enabled before setup was started.
    dialog.currentPageId_ = /*Auto-scan enabled=*/2;
    dialog.autoScanPreviouslyEnabled_ = true;
    setPrefData = [];

    dialog.onPreviousClick_();
    assertNotEquals(dialog.currentPageId_, /*Auto-scan enabled=*/2);

    // At no point should auto_scan be set to false.
    for (const data of setPrefData) {
      if (data.key === 'settings.a11y.switch_access.auto_scan.enabled') {
        assertTrue(data.value);
      }
    }
  });

  test('Auto-scan speed slower and faster buttons', function() {
    const setPrefData = [];
    // Mock this API to confirm it's getting called, and with the right values.
    chrome.settingsPrivate.setPref = function(key, value) {
      setPrefData.push({key, value});
    };

    assertTrue(dialog.$.switchAccessSetupGuideDialog.open);

    const slowerButton = dialog.$.autoScanSpeedSlower;
    assertTrue(!!slowerButton);

    slowerButton.click();
    assertEquals(1, setPrefData.length);
    assertEquals(
        'settings.a11y.switch_access.auto_scan.speed_ms', setPrefData[0].key);
    assertEquals(1100, setPrefData[0].value);

    const fasterButton = dialog.$.autoScanSpeedFaster;
    assertTrue(!!fasterButton);

    fasterButton.click();
    assertEquals(2, setPrefData.length);
    assertEquals(
        'settings.a11y.switch_access.auto_scan.speed_ms', setPrefData[1].key);
    assertEquals(900, setPrefData[1].value);
  });

  test('Illustration changes with switch count', function() {
    const switchCountIllustration =
        dialog['$']['choose-switch-count-illustration'];
    assertTrue(!!switchCountIllustration);
    assertEquals('illustration one-switch', switchCountIllustration.className);

    const switchCountGroup = dialog['$']['switch-count-group'];
    assertTrue(!!switchCountGroup);

    const twoSwitches = switchCountGroup.querySelector('[name="two-switches"]');
    assertTrue(!!twoSwitches);
    switchCountGroup.select_(twoSwitches);
    assertEquals(
        'illustration two-switches', switchCountIllustration.className);

    const threeSwitches =
        switchCountGroup.querySelector('[name="three-switches"]');
    assertTrue(!!threeSwitches);
    switchCountGroup.select_(threeSwitches);
    assertEquals(
        'illustration three-switches', switchCountIllustration.className);

    const oneSwitch = switchCountGroup.querySelector('[name="one-switch"]');
    assertTrue(!!oneSwitch);
    switchCountGroup.select_(oneSwitch);
    assertEquals('illustration one-switch', switchCountIllustration.className);
  });
});
