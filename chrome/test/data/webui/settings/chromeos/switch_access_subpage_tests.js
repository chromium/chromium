// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {Router, routes, SwitchAccessSubpageBrowserProxyImpl} from 'chrome://os-settings/os_settings.js';
import {webUIListenerCallback} from 'chrome://resources/ash/common/cr.m.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {getDeepActiveElement} from 'chrome://resources/ash/common/util.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertDeepEquals, assertEquals} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

class TestSwitchAccessSubpageBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'refreshAssignmentsFromPrefs',
      'notifySwitchAccessActionAssignmentPaneActive',
      'notifySwitchAccessActionAssignmentPaneInactive',
      'notifySwitchAccessSetupGuideAttached',
    ]);
  }

  /** @override */
  refreshAssignmentsFromPrefs() {
    this.methodCalled('refreshAssignmentsFromPrefs');
  }

  /** @override */
  notifySwitchAccessActionAssignmentPaneActive() {
    this.methodCalled('notifySwitchAccessActionAssignmentPaneActive');
  }

  /** @override */
  notifySwitchAccessActionAssignmentPaneInactive() {
    this.methodCalled('notifySwitchAccessActionAssignmentPaneInactive');
  }

  /** @override */
  notifySwitchAccessSetupGuideAttached() {
    this.methodCalled('notifySwitchAccessSetupGuideAttached');
  }
}

suite('SwitchAccessSubpageTests', function() {
  let page = null;
  let browserProxy = null;

  function getDefaultPrefs() {
    return {
      settings: {
        a11y: {
          switch_access: {
            auto_scan: {
              enabled: {
                key: 'settings.a11y.switch_access.auto_scan.enabled',
                type: chrome.settingsPrivate.PrefType.BOOLEAN,
                value: false,
              },
            },
            next: {
              device_key_codes: {
                key: 'settings.a11y.switch_access.next.device_key_codes',
                type: chrome.settingsPrivate.PrefType.DICTIONARY,
                value: {},
              },
            },
            previous: {
              device_key_codes: {
                key: 'settings.a11y.switch_access.previous.device_key_codes',
                type: chrome.settingsPrivate.PrefType.DICTIONARY,
                value: {},
              },
            },
            select: {
              device_key_codes: {
                key: 'settings.a11y.switch_access.select.device_key_codes',
                type: chrome.settingsPrivate.PrefType.DICTIONARY,
                value: {},
              },
            },
          },
        },
      },
    };
  }

  /** @param {?Object} prefs */
  function initPage(prefs) {
    page = document.createElement('settings-switch-access-subpage');
    page.prefs = prefs || getDefaultPrefs();

    document.body.appendChild(page);
  }

  setup(function() {
    browserProxy = new TestSwitchAccessSubpageBrowserProxy();
    SwitchAccessSubpageBrowserProxyImpl.setInstanceForTesting(browserProxy);

    PolymerTest.clearBody();
  });

  teardown(function() {
    if (page) {
      page.remove();
    }
    Router.getInstance().resetRouteForTesting();
  });

  /**
   * @param {!Array<string>} keys New switch key assignments for select action.
   * @return {string} Sub-label text from the select link row.
   */
  function getSublabelForSelectUpdates(keys) {
    webUIListenerCallback('switch-access-assignments-changed', {
      select: keys.map(key => ({key, device: 'usb'})),
      next: [],
      previous: [],
    });

    return page.shadowRoot.querySelector('#selectLinkRow')
        .shadowRoot.querySelector('#subLabel')
        .textContent.trim();
  }


  test('Switch assignment key display', function() {
    initPage();

    assertEquals(0, page.selectAssignments_.length);
    assertEquals(0, page.nextAssignments_.length);
    assertEquals(0, page.previousAssignments_.length);

    // Simulate a pref change for the select action.
    webUIListenerCallback(
        'switch-access-assignments-changed',
        {select: [{key: 'a', device: 'usb'}], next: [], previous: []});

    assertEquals(1, page.selectAssignments_.length);
    assertDeepEquals({key: 'a', device: 'usb'}, page.selectAssignments_[0]);
    assertEquals(0, page.nextAssignments_.length);
    assertEquals(0, page.previousAssignments_.length);
  });

  test('Switch assignment sub-labels', function() {
    initPage();

    assertEquals('0 switches assigned', getSublabelForSelectUpdates([]));
    assertEquals('Backspace (USB)', getSublabelForSelectUpdates(['Backspace']));
    assertEquals(
        'Backspace (USB), Tab (USB)',
        getSublabelForSelectUpdates(['Backspace', 'Tab']));
    assertEquals(
        'Backspace (USB), Tab (USB), Enter (USB)',
        getSublabelForSelectUpdates(['Backspace', 'Tab', 'Enter']));
    assertEquals(
        'Backspace (USB), Tab (USB), Enter (USB), ' +
            'and 1 more switch',
        getSublabelForSelectUpdates(['Backspace', 'Tab', 'Enter', 'a']));
    assertEquals(
        'Backspace (USB), Tab (USB), Enter (USB), ' +
            'and 2 more switches',
        getSublabelForSelectUpdates(['Backspace', 'Tab', 'Enter', 'a', 'b']));
    assertEquals(
        'Backspace (USB), Tab (USB), Enter (USB), ' +
            'and 3 more switches',
        getSublabelForSelectUpdates(
            ['Backspace', 'Tab', 'Enter', 'a', 'b', 'c']));
    assertEquals(
        'Backspace (USB), Tab (USB), Enter (USB), ' +
            'and 4 more switches',
        getSublabelForSelectUpdates(
            ['Backspace', 'Tab', 'Enter', 'a', 'b', 'c', 'd']));
  });

  test('Switch access action assignment dialog', async function() {
    initPage();

    // Simulate a click on the select link row.
    page.$.selectLinkRow.click();

    await browserProxy.whenCalled(
        'notifySwitchAccessActionAssignmentPaneActive');

    // Make sure we populate the initial |keyCodes_| state on the
    // SwitchAccessActionAssignmentDialog.
    webUIListenerCallback(
        'switch-access-assignments-changed',
        {select: [], next: [], previous: []});

    // Simulate pressing 'a' twice.
    webUIListenerCallback(
        'switch-access-got-key-press-for-assignment',
        {key: 'a', keyCode: 65, device: 'usb'});
    webUIListenerCallback(
        'switch-access-got-key-press-for-assignment',
        {key: 'a', keyCode: 65, device: 'usb'});

    // This should cause the dialog to close.
    await browserProxy.whenCalled(
        'notifySwitchAccessActionAssignmentPaneInactive');
  });

  test('Switch access action assignment dialog error state', async function() {
    initPage();

    // Simulate a click on the select link row.
    page.$.selectLinkRow.click();

    await browserProxy.whenCalled(
        'notifySwitchAccessActionAssignmentPaneActive');

    // Simulate pressing 'a', and then 'b'.
    webUIListenerCallback(
        'switch-access-got-key-press-for-assignment',
        {key: 'a', keyCode: 65, device: 'usb'});
    webUIListenerCallback(
        'switch-access-got-key-press-for-assignment',
        {key: 'b', keyCode: 66, device: 'usb'});

    const element =
        page.shadowRoot.querySelector('#switchAccessActionAssignmentDialog');
    await waitAfterNextRender(element);

    // This should update the error field at the bottom of the dialog.
    const errorText =
        page.shadowRoot.querySelector('#switchAccessActionAssignmentDialog')
            .shadowRoot.querySelector('#switchAccessActionAssignmentPane')
            .shadowRoot.querySelector('#error')
            .textContent.trim();
    assertEquals('Keys don’t match. Press any key to exit.', errorText);
  });

  test('Deep link to auto-scan keyboards', async () => {
    loadTimeData.overrideValues({
      showExperimentalAccessibilitySwitchAccessImprovedTextInput: true,
    });
    const prefs = getDefaultPrefs();
    prefs.settings.a11y.switch_access.auto_scan.enabled.value = true;
    initPage(prefs);

    flush();

    const params = new URLSearchParams();
    params.append('settingId', '1525');
    Router.getInstance().navigateTo(
        routes.MANAGE_SWITCH_ACCESS_SETTINGS, params);

    const deepLinkElement =
        page.shadowRoot.querySelector('#keyboardScanSpeedSlider')
            .shadowRoot.querySelector('cr-slider');
    await waitAfterNextRender(deepLinkElement);

    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Auto-scan keyboard toggle should be focused for settingId=1525.');
  });

  test('Warning dialog before clearing all switch assignments', async () => {
    loadTimeData.overrideValues({
      showSwitchAccessSetupGuide: true,
    });
    const prefs = getDefaultPrefs();
    prefs.settings.a11y.switch_access.select.device_key_codes.value = {
      25: 'usb',
    };
    initPage(prefs);

    // Mock this API to confirm it's getting called with the right values.
    const setPrefData = [];
    chrome.settingsPrivate.setPref = function(key, value) {
      setPrefData.push({key, value});
    };

    // Open the setup guide warning dialog.
    page.$.setupGuideLink.click();
    flush();

    // Check that the dialog is open.
    let warningDialog = page.shadowRoot.querySelector(
        'settings-switch-access-setup-guide-warning-dialog');
    assertTrue(!!warningDialog);

    // Press "cancel" to exit the dialog.
    const cancelButton = warningDialog.$.cancel;
    assertTrue(!!cancelButton);
    cancelButton.click();
    flush();

    // Check that the dialog is closed, and the setup guide is not open.
    warningDialog = page.shadowRoot.querySelector(
        'settings-switch-access-setup-guide-warning-dialog');
    assertFalse(!!warningDialog);
    let setupDialog = page.shadowRoot.querySelector(
        'settings-switch-access-setup-guide-dialog');

    assertFalse(!!setupDialog);

    // Re-open the warning dialog.
    page.$.setupGuideLink.click();
    flush();
    warningDialog = page.shadowRoot.querySelector(
        'settings-switch-access-setup-guide-warning-dialog');
    assertTrue(!!warningDialog);

    // Press "continue" to open the setup guide.
    const continueButton = warningDialog.$.continue;
    assertTrue(!!continueButton);
    continueButton.click();
    flush();
    await browserProxy.whenCalled('notifySwitchAccessSetupGuideAttached');

    // Check that the setup guide has opened.
    setupDialog = page.shadowRoot.querySelector(
        'settings-switch-access-setup-guide-dialog');
    assertTrue(!!setupDialog);

    // Check that the switch assignments have been cleared.
    const setSelectData =
        setPrefData.find(entry => entry.key.includes('select'));
    assertTrue(!!setSelectData);
    // Two empty objects will not be equal, so check the number of keys to
    // confirm the assignments have been cleared.
    assertEquals(0, Object.keys(setSelectData.value).length);

    const setNextData = setPrefData.find(entry => entry.key.includes('next'));
    assertTrue(!!setNextData);
    assertEquals(0, Object.keys(setNextData.value).length);

    const setPreviousData =
        setPrefData.find(entry => entry.key.includes('previous'));
    assertTrue(!!setPreviousData);
    assertEquals(0, Object.keys(setPreviousData.value).length);
  });

  test(
      'Setup guide starts automatically if no switches are assigned',
      async () => {
        loadTimeData.overrideValues({
          showSwitchAccessSetupGuide: true,
        });

        initPage();
        // Normally on startup, the browser proxy calls a C++ function,
        // which then fires an event that calls this function.
        page.onAssignmentsChanged_({select: [], next: [], previous: []});
        flush();
        await browserProxy.whenCalled('notifySwitchAccessSetupGuideAttached');

        const setupDialog = page.shadowRoot.querySelector(
            'settings-switch-access-setup-guide-dialog');
        assertTrue(!!setupDialog);
      });
});
