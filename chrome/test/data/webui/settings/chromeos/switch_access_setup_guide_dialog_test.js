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
              'enabled': false,
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

    const previousButton = dialog.$.previous;
    assertTrue(!!previousButton);
    previousButton.click();

    assertEquals(/*Auto-scan enabled=*/2, dialog.currentPageId_);
    previousButton.click();

    assertEquals(/*Assign select=*/1, dialog.currentPageId_);
    previousButton.click();

    assertEquals(/*Intro=*/0, dialog.currentPageId_);
  });

  test('Page contents are hidden and shown as expected', function() {
    assertTrue(dialog.$.switchAccessSetupGuideDialog.open);

    dialog.loadPage_(/*Intro=*/0);

    // Verify the contents of the Intro page.
    assertFalse(dialog.$.bluetooth.hidden);
    assertFalse(dialog.$.exit.hidden);
    assertFalse(dialog.$.next.hidden);
    assertTrue(dialog.$.previous.hidden);
    assertFalse(dialog['$']['intro']['hidden']);
    assertTrue(dialog['$']['assign-select']['hidden']);
    assertTrue(dialog['$']['auto-scan-enabled']['hidden']);
    assertTrue(dialog['$']['choose-switch-count']['hidden']);

    dialog.loadPage_(/*Assign select=*/1);

    // Verify the contents of the assign select page.
    assertTrue(dialog.$.bluetooth.hidden);
    assertTrue(dialog.$.exit.hidden);
    assertFalse(dialog.$.next.hidden);
    assertFalse(dialog.$.previous.hidden);
    assertTrue(dialog['$']['intro']['hidden']);
    assertFalse(dialog['$']['assign-select']['hidden']);
    assertTrue(dialog['$']['auto-scan-enabled']['hidden']);
    assertTrue(dialog['$']['choose-switch-count']['hidden']);

    dialog.loadPage_(/*Auto-scan enabled=*/2);

    // Verify the contents of the auto-scan enabled page.
    assertTrue(dialog.$.bluetooth.hidden);
    assertTrue(dialog.$.exit.hidden);
    assertFalse(dialog.$.next.hidden);
    assertFalse(dialog.$.previous.hidden);
    assertTrue(dialog['$']['intro']['hidden']);
    assertTrue(dialog['$']['assign-select']['hidden']);
    assertFalse(dialog['$']['auto-scan-enabled']['hidden']);
    assertTrue(dialog['$']['choose-switch-count']['hidden']);

    dialog.loadPage_(/*Choose switch count=*/3);

    // Verify the contents of the choose switch count page.
    assertTrue(dialog.$.bluetooth.hidden);
    assertFalse(dialog.$.exit.hidden);
    assertTrue(dialog.$.next.hidden);
    assertFalse(dialog.$.previous.hidden);
    assertTrue(dialog['$']['intro']['hidden']);
    assertTrue(dialog['$']['assign-select']['hidden']);
    assertTrue(dialog['$']['auto-scan-enabled']['hidden']);
    assertFalse(dialog['$']['choose-switch-count']['hidden']);
  });

  test('Auto-scan enabled and disabled correctly', function() {
    let setPrefData = [];
    // Mock this API to confirm it's getting called, and with the right values.
    chrome.settingsPrivate.setPref = function(key, value) {
      setPrefData.push({key, value});
    };

    assertTrue(dialog.$.switchAccessSetupGuideDialog.open);
    assertFalse(dialog.prefs.settings.a11y.switch_access.auto_scan.enabled);

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
});
