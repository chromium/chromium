// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {webUIListenerCallback} from 'chrome://resources/ash/common/cr.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

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
            'next': {
              'device_key_codes': {
                value: {},
              },
            },
            'previous': {
              'device_key_codes': {
                value: {},
              },
            },
            'select': {
              'device_key_codes': {
                value: {},
              },
            },
          },
        },
      },
    };
    document.body.appendChild(dialog);
    flush();
  });

  test('Exit button closes dialog', function() {
    assertTrue(dialog.$.switchAccessSetupGuideDialog.open);

    const exitButton = dialog.$.exit;
    assertTrue(!!exitButton);

    exitButton.click();
    assertFalse(dialog.$.switchAccessSetupGuideDialog.open);
  });

  test('Navigation between dialog pages (1 switch)', function() {
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
    assertEquals(1, dialog.switchCount_);
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

    const startOverButton = dialog.$.startOver;
    assertTrue(!!startOverButton);
    startOverButton.click();

    assertEquals(/*Intro=*/0, dialog.currentPageId_);
  });

  test('Navigation between dialog pages (2 switches)', function () {
    assertTrue(dialog.$.switchAccessSetupGuideDialog.open);
    assertEquals(/*Intro=*/0, dialog.currentPageId_);
    dialog.switchCount_ = 2;

    const nextButton = dialog.$.next;
    assertTrue(!!nextButton);
    nextButton.click();

    assertEquals(/*Assign select=*/1, dialog.currentPageId_);
    nextButton.click();

    assertEquals(/*Auto-scan enabled=*/2, dialog.currentPageId_);
    nextButton.click();

    assertEquals(/*Choose switch count=*/3, dialog.currentPageId_);
    nextButton.click();

    assertEquals(/*Assign next=*/5, dialog.currentPageId_);
    nextButton.click();

    assertEquals(/*Closing=*/8, dialog.currentPageId_);

    const previousButton = dialog.$.previous;
    assertTrue(!!previousButton);
    previousButton.click();

    assertEquals(/*Assign next=*/5, dialog.currentPageId_);
    previousButton.click();

    assertEquals(/*Choose switch count=*/3, dialog.currentPageId_);
    previousButton.click();

    assertEquals(/*Auto-scan enabled=*/2, dialog.currentPageId_);
    previousButton.click();

    assertEquals(/*Assign select=*/1, dialog.currentPageId_);
    previousButton.click();

    assertEquals(/*Intro=*/0, dialog.currentPageId_);
  });

  test('Navigation between dialog pages (3 switches)', function () {
    assertTrue(dialog.$.switchAccessSetupGuideDialog.open);
    assertEquals(/*Intro=*/0, dialog.currentPageId_);
    dialog.switchCount_ = 3;

    const nextButton = dialog.$.next;
    assertTrue(!!nextButton);
    nextButton.click();

    assertEquals(/*Assign select=*/1, dialog.currentPageId_);
    nextButton.click();

    assertEquals(/*Auto-scan enabled=*/2, dialog.currentPageId_);
    nextButton.click();

    assertEquals(/*Choose switch count=*/3, dialog.currentPageId_);
    nextButton.click();

    assertEquals(/*Assign next=*/5, dialog.currentPageId_);
    nextButton.click();

    assertEquals(/*Assign previous=*/6, dialog.currentPageId_);
    nextButton.click();

    assertEquals(/*Closing=*/8, dialog.currentPageId_);

    const previousButton = dialog.$.previous;
    assertTrue(!!previousButton);
    previousButton.click();

    assertEquals(/*Assign previous=*/6, dialog.currentPageId_);
    previousButton.click();

    assertEquals(/*Assign next=*/5, dialog.currentPageId_);
    previousButton.click();

    assertEquals(/*Choose switch count=*/3, dialog.currentPageId_);
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
    assertFalse(dialog.$.intro.hidden);

    dialog.loadPage_(/*Assign select=*/1);

    // Verify the contents of the assign select page.
    assertTrue(dialog.$.intro.hidden);
    assertFalse(dialog.$.assignSwitch.hidden);

    dialog.loadPage_(/*Auto-scan enabled=*/2);

    // Verify the contents of the auto-scan enabled page.
    assertTrue(dialog.$.assignSwitch.hidden);
    assertFalse(dialog.$.autoScanEnabled.hidden);

    dialog.loadPage_(/*Choose switch count=*/3);

    // Verify the contents of the choose switch count page.
    assertTrue(dialog.$.autoScanEnabled.hidden);
    assertFalse(dialog.$.chooseSwitchCount.hidden);

    dialog.loadPage_(/*Auto-scan speed=*/4);

    // Verify the contents of the auto-scan speed page.
    assertTrue(dialog.$.chooseSwitchCount.hidden);
    assertFalse(dialog.$.autoScanSpeed.hidden);

    dialog.loadPage_(/*Assign next=*/ 5);

    // Verify the contents of the assign next page.
    assertTrue(dialog.$.autoScanSpeed.hidden);
    assertFalse(dialog.$.assignSwitch.hidden);

    dialog.loadPage_(/*Assign previous=*/ 6);

    // Verify the contents of the assign previous page.
    assertFalse(dialog.$.assignSwitch.hidden);

    dialog.loadPage_(/*Closing=*/8);

    // Verify the contents of the closing page.
    assertTrue(dialog.$.autoScanSpeed.hidden);
    assertTrue(dialog.$.assignSwitch.hidden);
    assertFalse(dialog.$.closing.hidden);
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

    // Confirm that auto-scan is disabled upon reaching the "Next" assignment
    // page.
    setPrefData = [];
    dialog.currentPageId_ = /*Choose switch count=*/ 3;
    dialog.switchCount_ = 2;
    dialog.onNextClick_();

    // Loading the assignment pane generates additional calls to setPref, so
    // expect at least one call to that function.
    assertLE(1, setPrefData.length);

    // Auto-scan enabled should be set to false at least once, and should not be
    // set to true.
    let autoScanEnabledSet = false;
    for (const data of setPrefData) {
      if (data.key === 'settings.a11y.switch_access.auto_scan.enabled') {
        autoScanEnabledSet = true;
        assertFalse(data.value);
      }
    }
    assertTrue(autoScanEnabledSet);
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
    const chooseSwitchCountEl =
        dialog.shadowRoot.querySelector('#chooseSwitchCount');
    assertTrue(!!chooseSwitchCountEl);
    assertEquals('1', chooseSwitchCountEl.getAttribute('data-switch-count'));

    const switchCountGroup =
        dialog.shadowRoot.querySelector('#switchCountGroup');
    assertTrue(!!switchCountGroup);

    const twoSwitches = switchCountGroup.querySelector('[name="two-switches"]');
    assertTrue(!!twoSwitches);
    switchCountGroup.select_(twoSwitches);
    assertEquals('2', chooseSwitchCountEl.getAttribute('data-switch-count'));

    const threeSwitches =
        switchCountGroup.querySelector('[name="three-switches"]');
    assertTrue(!!threeSwitches);
    switchCountGroup.select_(threeSwitches);
    assertEquals('3', chooseSwitchCountEl.getAttribute('data-switch-count'));

    const oneSwitch = switchCountGroup.querySelector('[name="one-switch"]');
    assertTrue(!!oneSwitch);
    switchCountGroup.select_(oneSwitch);
    assertEquals('1', chooseSwitchCountEl.getAttribute('data-switch-count'));
  });

  test('Assignment pane behaves correctly', function() {
    assertTrue(dialog.$.switchAccessSetupGuideDialog.open);
    dialog.switchCount_ = 3;

    const assignContents =
        dialog.$.assignSwitch.querySelector('.sa-setup-contents');
    assertTrue(!!assignContents);
    // Check that there is no pane currently attached.
    assertEquals(0, assignContents.children.length);

    const nextButton = dialog.$.next;
    assertTrue(!!nextButton);
    nextButton.click();

    // Confirm that the pane loaded successfully.
    assertEquals(1, assignContents.children.length);
    assertEquals('select', assignContents.firstChild.action);

    // Simulate the pane exiting without successfully assigning a switch.
    webUIListenerCallback('exit-pane');

    // Confirm the page has not changed and the pane was loaded.
    assertEquals(/*Assign select=*/ 1, dialog.currentPageId_);
    assertEquals(1, assignContents.children.length);
    assertEquals('select', assignContents.firstChild.action);

    // Simulate the user successfully assigning a switch.
    // TODO(anastasi): The change to the pref should correspond to the observer
    // being called automatically. Investigate.
    dialog.prefs.settings.a11y.switch_access.select.device_key_codes.value =
        {23: 'usb'};
    dialog.onSwitchAssignmentMaybeChanged_();

    // Confirm that we're on the next page.
    assertEquals(/*Auto-scan enabled=*/ 2, dialog.currentPageId_);
    assertEquals(0, assignContents.children.length);

    nextButton.click();
    nextButton.click();

    // Confirm that the pane loaded successfully.
    assertEquals(/*Assign next=*/ 5, dialog.currentPageId_);
    assertEquals(1, assignContents.children.length);
    assertEquals('next', assignContents.firstChild.action);

    // Simulate the user successfully assigning a switch.
    dialog.prefs.settings.a11y.switch_access.next.device_key_codes.value =
        {101: 'bluetooth'};
    dialog.onSwitchAssignmentMaybeChanged_();

    // Confirm that we're on the page to assign previous, and that there's only
    // one dialog.
    assertEquals(/*Assign previous=*/ 6, dialog.currentPageId_);
    assertEquals(1, assignContents.children.length);
    assertEquals('previous', assignContents.firstChild.action);
  });
});
