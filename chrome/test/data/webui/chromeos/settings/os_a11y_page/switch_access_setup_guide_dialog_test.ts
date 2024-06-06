// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';
import 'chrome://os-settings/os_settings.js';

import {SettingsSwitchAccessActionAssignmentPaneElement, SettingsSwitchAccessSetupGuideDialogElement} from 'chrome://os-settings/lazy_load.js';
import {CrRadioGroupElement, Router, routes} from 'chrome://os-settings/os_settings.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertLE, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('<settings-switch-access-setup-guide-dialog>', () => {
  let dialog: SettingsSwitchAccessSetupGuideDialogElement;

  interface Prefs {
    key: string;
    value: boolean|number;
  }

  setup(() => {
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

  teardown(() => {
    dialog.remove();
  });

  test('Exit button closes dialog', () => {
    assertTrue(dialog.$.switchAccessSetupGuideDialog.open);

    const exitButton =
        dialog.shadowRoot!.querySelector<HTMLButtonElement>('#exit');
    assertTrue(!!exitButton);

    exitButton.click();
    assertFalse(dialog.$.switchAccessSetupGuideDialog.open);
  });

  test('Navigation between dialog pages (1 switch)', () => {
    assertTrue(dialog.$.switchAccessSetupGuideDialog.open);
    assertEquals(/*Intro=*/ 0, dialog.get('currentPageId_'));

    const nextButton =
        dialog.shadowRoot!.querySelector<HTMLButtonElement>('#next');
    assertTrue(!!nextButton);
    nextButton.click();

    assertEquals(/*Assign select=*/ 1, dialog.get('currentPageId_'));
    nextButton.click();

    assertEquals(/*Auto-scan enabled=*/ 2, dialog.get('currentPageId_'));
    nextButton.click();

    assertEquals(/*Choose switch count=*/ 3, dialog.get('currentPageId_'));
    assertEquals(1, dialog.get('switchCount_'));
    nextButton.click();

    assertEquals(/*Auto-scan speed=*/ 4, dialog.get('currentPageId_'));
    nextButton.click();

    assertEquals(/*Closing=*/ 8, dialog.get('currentPageId_'));

    const previousButton =
        dialog.shadowRoot!.querySelector<HTMLButtonElement>('#previous');
    assertTrue(!!previousButton);
    previousButton.click();

    assertEquals(/*Auto-scan speed=*/ 4, dialog.get('currentPageId_'));
    previousButton.click();

    assertEquals(/*Choose switch count=*/ 3, dialog.get('currentPageId_'));
    previousButton.click();

    assertEquals(/*Auto-scan enabled=*/ 2, dialog.get('currentPageId_'));
    previousButton.click();

    assertEquals(/*Assign select=*/ 1, dialog.get('currentPageId_'));
    previousButton.click();

    assertEquals(/*Intro=*/ 0, dialog.get('currentPageId_'));

    // Check that the "Start Over" button takes us from the closing page back to
    // the intro.
    dialog.set('currentPageId_', /*Closing=*/ 8);

    const startOverButton =
        dialog.shadowRoot!.querySelector<HTMLButtonElement>('#startOver');
    assertTrue(!!startOverButton);
    startOverButton.click();

    assertEquals(/*Intro=*/ 0, dialog.get('currentPageId_'));
  });

  test('Navigation between dialog pages (2 switches)', () => {
    assertTrue(dialog.$.switchAccessSetupGuideDialog.open);
    assertEquals(/*Intro=*/ 0, dialog.get('currentPageId_'));
    dialog.set('switchCount_', 2);

    const nextButton =
        dialog.shadowRoot!.querySelector<HTMLButtonElement>('#next');
    assertTrue(!!nextButton);
    nextButton.click();

    assertEquals(/*Assign select=*/ 1, dialog.get('currentPageId_'));
    nextButton.click();

    assertEquals(/*Auto-scan enabled=*/ 2, dialog.get('currentPageId_'));
    nextButton.click();

    assertEquals(/*Choose switch count=*/ 3, dialog.get('currentPageId_'));
    nextButton.click();

    assertEquals(/*Assign next=*/ 5, dialog.get('currentPageId_'));
    nextButton.click();

    assertEquals(/*Closing=*/ 8, dialog.get('currentPageId_'));

    const previousButton =
        dialog.shadowRoot!.querySelector<HTMLButtonElement>('#previous');
    assertTrue(!!previousButton);
    previousButton.click();

    assertEquals(/*Assign next=*/ 5, dialog.get('currentPageId_'));
    previousButton.click();

    assertEquals(/*Choose switch count=*/ 3, dialog.get('currentPageId_'));
    previousButton.click();

    assertEquals(/*Auto-scan enabled=*/ 2, dialog.get('currentPageId_'));
    previousButton.click();

    assertEquals(/*Assign select=*/ 1, dialog.get('currentPageId_'));
    previousButton.click();

    assertEquals(/*Intro=*/ 0, dialog.get('currentPageId_'));
  });

  test('Navigation between dialog pages (3 switches)', () => {
    assertTrue(dialog.$.switchAccessSetupGuideDialog.open);
    assertEquals(/*Intro=*/ 0, dialog.get('currentPageId_'));
    dialog.set('switchCount_', 3);

    const nextButton =
        dialog.shadowRoot!.querySelector<HTMLButtonElement>('#next');
    assertTrue(!!nextButton);
    nextButton.click();

    assertEquals(/*Assign select=*/ 1, dialog.get('currentPageId_'));
    nextButton.click();

    assertEquals(/*Auto-scan enabled=*/ 2, dialog.get('currentPageId_'));
    nextButton.click();

    assertEquals(/*Choose switch count=*/ 3, dialog.get('currentPageId_'));
    nextButton.click();

    assertEquals(/*Assign next=*/ 5, dialog.get('currentPageId_'));
    nextButton.click();

    assertEquals(/*Assign previous=*/ 6, dialog.get('currentPageId_'));
    nextButton.click();

    assertEquals(/*Closing=*/ 8, dialog.get('currentPageId_'));

    const previousButton =
        dialog.shadowRoot!.querySelector<HTMLButtonElement>('#previous');
    assertTrue(!!previousButton);
    previousButton.click();

    assertEquals(/*Assign previous=*/ 6, dialog.get('currentPageId_'));
    previousButton.click();

    assertEquals(/*Assign next=*/ 5, dialog.get('currentPageId_'));
    previousButton.click();

    assertEquals(/*Choose switch count=*/ 3, dialog.get('currentPageId_'));
    previousButton.click();

    assertEquals(/*Auto-scan enabled=*/ 2, dialog.get('currentPageId_'));
    previousButton.click();

    assertEquals(/*Assign select=*/ 1, dialog.get('currentPageId_'));
    previousButton.click();

    assertEquals(/*Intro=*/ 0, dialog.get('currentPageId_'));
  });

  test('Page contents are hidden and shown as expected', () => {
    assertTrue(dialog.$.switchAccessSetupGuideDialog.open);

    dialog['loadPage_'](/*Intro=*/ 0);

    // Verify the contents of the Intro page.
    let introEl = dialog.shadowRoot!.querySelector<HTMLElement>('#intro');
    assertTrue(!!introEl);
    assertFalse(introEl.hidden);

    dialog['loadPage_'](/*Assign select=*/ 1);

    // Verify the contents of the assign select page.
    introEl = dialog.shadowRoot!.querySelector<HTMLElement>('#intro');
    assertTrue(!!introEl);
    assertTrue(introEl.hidden);
    let assignSwitch =
        dialog.shadowRoot!.querySelector<HTMLElement>('#assignSwitch');
    assertTrue(!!assignSwitch);
    assertFalse(assignSwitch.hidden);

    dialog['loadPage_'](/*Auto-scan enabled=*/ 2);

    // Verify the contents of the auto-scan enabled page.
    assignSwitch =
        dialog.shadowRoot!.querySelector<HTMLElement>('#assignSwitch');
    assertTrue(!!assignSwitch);
    assertTrue(assignSwitch.hidden);
    let autoScanEnabled =
        dialog.shadowRoot!.querySelector<HTMLElement>('#autoScanEnabled');
    assertTrue(!!autoScanEnabled);
    assertFalse(autoScanEnabled.hidden);

    dialog['loadPage_'](/*Choose switch count=*/ 3);

    // Verify the contents of the choose switch count page.
    autoScanEnabled =
        dialog.shadowRoot!.querySelector<HTMLElement>('#autoScanEnabled');
    assertTrue(!!autoScanEnabled);
    assertTrue(autoScanEnabled.hidden);
    assertFalse(dialog.$.chooseSwitchCount.hidden);

    dialog['loadPage_'](/*Auto-scan speed=*/ 4);

    // Verify the contents of the auto-scan speed page.
    assertTrue(dialog.$.chooseSwitchCount.hidden);
    let autoScanSpeed =
        dialog.shadowRoot!.querySelector<HTMLElement>('#autoScanSpeed');
    assertTrue(!!autoScanSpeed);
    assertFalse(autoScanSpeed.hidden);

    dialog['loadPage_'](/*Assign next=*/ 5);

    // Verify the contents of the assign next page.
    autoScanSpeed =
        dialog.shadowRoot!.querySelector<HTMLElement>('#autoScanSpeed');
    assertTrue(!!autoScanSpeed);
    assertTrue(autoScanSpeed.hidden);
    assignSwitch =
        dialog.shadowRoot!.querySelector<HTMLElement>('#assignSwitch');
    assertTrue(!!assignSwitch);
    assertFalse(assignSwitch.hidden);

    dialog['loadPage_'](/*Assign previous=*/ 6);

    // Verify the contents of the assign previous page.
    assignSwitch =
        dialog.shadowRoot!.querySelector<HTMLElement>('#assignSwitch');
    assertTrue(!!assignSwitch);
    assertFalse(assignSwitch.hidden);

    dialog['loadPage_'](/*Closing=*/ 8);

    // Verify the contents of the closing page.
    autoScanSpeed =
        dialog.shadowRoot!.querySelector<HTMLElement>('#autoScanSpeed');
    assertTrue(!!autoScanSpeed);
    assertTrue(autoScanSpeed.hidden);
    assignSwitch =
        dialog.shadowRoot!.querySelector<HTMLElement>('#assignSwitch');
    assertTrue(!!assignSwitch);
    assertTrue(assignSwitch.hidden);
    const closingEl = dialog.shadowRoot!.querySelector<HTMLElement>('#closing');
    assertTrue(!!closingEl);
    assertFalse(closingEl.hidden);
  });

  test('Auto-scan enabled and disabled correctly', () => {
    let setPrefData: Prefs[] = [];

    // Mock this API to confirm it's getting called, and with the right values.
    chrome.settingsPrivate.setPref = function(key, value) {
      setPrefData.push({key, value});
      return Promise.resolve(true);
    };

    assertTrue(dialog.$.switchAccessSetupGuideDialog.open);
    assertFalse(
        dialog.prefs.settings.a11y.switch_access.auto_scan.enabled.value);

    // Mock that we are on the page before auto scan is enabled.
    dialog.set('currentPageId_', /*Assign select=*/ 1);

    // Moving forward should enable auto-scan.
    dialog['onNextClick_']();
    assertEquals(/*Auto-scan enabled=*/ 2, dialog.get('currentPageId_'));

    // Check that the settings API was called with the correct parameters.
    assertEquals(/*Assign select=*/ 1, setPrefData.length);
    assertEquals(
        'settings.a11y.switch_access.auto_scan.enabled', setPrefData[0]!.key);
    assertEquals(true, setPrefData[0]!.value);

    // Moving backward should disable auto-scan.
    dialog['onPreviousClick_']();
    assertNotEquals(/*Auto-scan enabled=*/ 2, dialog.get('currentPageId_'));

    assertEquals(2, setPrefData.length);
    assertEquals(
        'settings.a11y.switch_access.auto_scan.enabled', setPrefData[1]!.key);
    assertEquals(false, setPrefData[1]!.value);

    // Confirm that auto-scan is disabled upon reaching the "Next" assignment
    // page.
    setPrefData = [];
    dialog.set('currentPageId_', /*Choose switch count=*/ 3);
    dialog.set('switchCount_', 2);
    dialog['onNextClick_']();

    // Loading the assignment pane generates additional calls to setPref, so
    // expect at least one call to that function.
    assertLE(1, setPrefData.length);

    // Auto-scan enabled should be set to false at least once, and should not be
    // set to true.
    let autoScanEnabledSet = false;
    for (const data of setPrefData) {
      if (data.key === 'settings.a11y.switch_access.auto_scan.enabled') {
        autoScanEnabledSet = true;
        assertEquals(false, data.value);
      }
    }
    assertTrue(autoScanEnabledSet);
  });

  test('Auto-scan speed slower and faster buttons', () => {
    const setPrefData: Prefs[] = [];

    // Mock this API to confirm it's getting called, and with the right values.
    chrome.settingsPrivate.setPref = function(key, value: number) {
      setPrefData.push({key, value});
      return Promise.resolve(true);
    };

    assertTrue(dialog.$.switchAccessSetupGuideDialog.open);

    const slowerButton = dialog.shadowRoot!.querySelector<HTMLButtonElement>(
        '#autoScanSpeedSlower');
    assertTrue(!!slowerButton);

    slowerButton.click();
    assertEquals(1, setPrefData.length);
    assertEquals(
        'settings.a11y.switch_access.auto_scan.speed_ms', setPrefData[0]!.key);
    assertEquals(1100, setPrefData[0]!.value);

    const fasterButton = dialog.shadowRoot!.querySelector<HTMLButtonElement>(
        '#autoScanSpeedFaster');
    assertTrue(!!fasterButton);

    fasterButton.click();
    assertEquals(2, setPrefData.length);
    assertEquals(
        'settings.a11y.switch_access.auto_scan.speed_ms', setPrefData[1]!.key);
    assertEquals(900, setPrefData[1]!.value);
  });

  test('Illustration changes with switch count', () => {
    const chooseSwitchCountEl =
        dialog.shadowRoot!.querySelector('#chooseSwitchCount');
    assertTrue(!!chooseSwitchCountEl);
    assertEquals('1', chooseSwitchCountEl.getAttribute('data-switch-count'));

    const switchCountGroup =
        dialog.shadowRoot!.querySelector<CrRadioGroupElement>(
            '#switchCountGroup');
    assertTrue(!!switchCountGroup);

    const twoSwitches = switchCountGroup.querySelector('[name="two-switches"]');
    assertTrue(!!twoSwitches);
    switchCountGroup['select_'](twoSwitches);
    assertEquals('2', chooseSwitchCountEl.getAttribute('data-switch-count'));

    const threeSwitches =
        switchCountGroup.querySelector('[name="three-switches"]');
    assertTrue(!!threeSwitches);
    switchCountGroup['select_'](threeSwitches);
    assertEquals('3', chooseSwitchCountEl.getAttribute('data-switch-count'));

    const oneSwitch = switchCountGroup.querySelector('[name="one-switch"]');
    assertTrue(!!oneSwitch);
    switchCountGroup['select_'](oneSwitch);
    assertEquals('1', chooseSwitchCountEl.getAttribute('data-switch-count'));
  });

  test('Assignment pane behaves correctly', () => {
    assertTrue(dialog.$.switchAccessSetupGuideDialog.open);
    dialog.set('switchCount_', 3);

    const assignSwitch = dialog.shadowRoot!.querySelector('#assignSwitch');
    assertTrue(!!assignSwitch);
    const assignContents = assignSwitch.querySelector('.sa-setup-contents');
    assertTrue(!!assignContents);
    // Check that there is no pane currently attached.
    assertEquals(0, assignContents.children.length);

    const nextButton =
        dialog.shadowRoot!.querySelector<HTMLButtonElement>('#next');
    assertTrue(!!nextButton);
    nextButton.click();

    // Confirm that the pane loaded successfully.
    assertEquals(1, assignContents.children.length);
    assertEquals(
        'select',
        (assignContents.firstChild as
         SettingsSwitchAccessActionAssignmentPaneElement)
            .action);

    // Simulate the pane exiting without successfully assigning a switch.
    webUIListenerCallback('exit-pane');

    // Confirm the page has not changed and the pane was loaded.
    assertEquals(/*Assign select=*/ 1, dialog.get('currentPageId_'));
    assertEquals(1, assignContents.children.length);
    assertEquals(
        'select',
        (assignContents.firstChild as
         SettingsSwitchAccessActionAssignmentPaneElement)
            .action);

    // Simulate the user successfully assigning a switch.
    // TODO(anastasi): The change to the pref should correspond to the observer
    // being called automatically. Investigate.
    dialog.prefs.settings.a11y.switch_access.select.device_key_codes.value = {
      23: 'usb',
    };
    dialog['onSwitchAssignmentMaybeChanged_']();

    // Confirm that we're on the next page.
    assertEquals(/*Auto-scan enabled=*/ 2, dialog.get('currentPageId_'));
    assertEquals(0, assignContents.children.length);

    nextButton.click();
    nextButton.click();

    // Confirm that the pane loaded successfully.
    assertEquals(/*Assign next=*/ 5, dialog.get('currentPageId_'));
    assertEquals(1, assignContents.children.length);
    assertEquals(
        'next',
        (assignContents.firstChild as
         SettingsSwitchAccessActionAssignmentPaneElement)
            .action);

    // Simulate the user successfully assigning a switch.
    dialog.prefs.settings.a11y.switch_access.next.device_key_codes.value = {
      101: 'bluetooth',
    };
    dialog['onSwitchAssignmentMaybeChanged_']();

    // Confirm that we're on the page to assign previous, and that there's only
    // one dialog.
    assertEquals(/*Assign previous=*/ 6, dialog.get('currentPageId_'));
    assertEquals(1, assignContents.children.length);
    assertEquals(
        'previous',
        (assignContents.firstChild as
         SettingsSwitchAccessActionAssignmentPaneElement)
            .action);
  });

  test('setup guide dialog closes after navigating to bluetooth page', () => {
    assertTrue(dialog.$.switchAccessSetupGuideDialog.open);

    const bluetoothButton =
        dialog.shadowRoot!.querySelector<HTMLButtonElement>('#bluetooth');
    assertTrue(!!bluetoothButton);

    bluetoothButton.click();
    assertFalse(dialog.$.switchAccessSetupGuideDialog.open);

    assertEquals(routes.BLUETOOTH_DEVICES, Router.getInstance().currentRoute);
  });
});
