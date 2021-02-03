// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/lazy_load.js';

// #import {SwitchAccessSubpageBrowserProxyImpl, SwitchAccessSubpageBrowserProxy, routes, Router} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
// #import {TestBrowserProxy} from '../../test_browser_proxy.m.js';
// #import {assertEquals, assertDeepEquals} from '../../chai_assert.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
// #import {waitAfterNextRender} from 'chrome://test/test_util.m.js';
// clang-format on

/**
 * @implements {SwitchAccessSubpageBrowserProxy}
 */
class TestSwitchAccessSubpageBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'refreshAssignmentsFromPrefs',
      'notifySwitchAccessActionAssignmentDialogAttached',
      'notifySwitchAccessActionAssignmentDialogDetached',
    ]);
  }

  /** @override */
  refreshAssignmentsFromPrefs() {
    this.methodCalled('refreshAssignmentsFromPrefs');
  }

  /** @override */
  notifySwitchAccessActionAssignmentDialogAttached() {
    this.methodCalled('notifySwitchAccessActionAssignmentDialogAttached');
  }

  /** @override */
  notifySwitchAccessActionAssignmentDialogDetached() {
    this.methodCalled('notifySwitchAccessActionAssignmentDialogDetached');
  }
}

suite('ManageAccessibilityPageTests', function() {
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
              }
            },
            next: {
              setting: {
                key: 'settings.a11y.switch_access.next.setting',
                type: chrome.settingsPrivate.PrefType.NUMBER,
                value: 0
              }
            },
            previous: {
              setting: {
                key: 'settings.a11y.switch_access.previous.setting',
                type: chrome.settingsPrivate.PrefType.NUMBER,
                value: 0
              }
            },
            select: {
              setting: {
                key: 'settings.a11y.switch_access.select.setting',
                type: chrome.settingsPrivate.PrefType.NUMBER,
                value: 0
              }
            },
          }
        }
      }
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
    SwitchAccessSubpageBrowserProxyImpl.instance_ = browserProxy;

    PolymerTest.clearBody();
  });

  teardown(function() {
    if (page) {
      page.remove();
    }
    settings.Router.getInstance().resetRouteForTesting();
  });

  /**
   * @param {!Array<string>} keys New switch key assignments for select action.
   * @return {string} Sub-label text from the select link row.
   */
  function getSublabelForSelectUpdates(keys) {
    cr.webUIListenerCallback('switch-access-assignments-changed', {
      select: keys.map(key => ({key, device: 'usb'})),
      next: [],
      previous: []
    });

    return page.$$('#selectLinkRow').$$('#subLabel').textContent.trim();
  }


  test('Switch assignment key display', function() {
    initPage();

    assertEquals(0, page.selectAssignments_.length);
    assertEquals(0, page.nextAssignments_.length);
    assertEquals(0, page.previousAssignments_.length);

    // Simulate a pref change for the select action.
    cr.webUIListenerCallback(
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

    await browserProxy.methodCalled(
        'notifySwitchAccessActionAssignmentDialogAttached');

    // Make sure we populate the initial |keyCodes_| state on the
    // SwitchAccessActionAssignmentDialog.
    cr.webUIListenerCallback(
        'switch-access-assignments-changed',
        {select: [], next: [], previous: []});

    // Simulate pressing 'a' twice.
    cr.webUIListenerCallback(
        'switch-access-got-key-press-for-assignment',
        {key: 'a', keyCode: 65, device: 'usb'});
    cr.webUIListenerCallback(
        'switch-access-got-key-press-for-assignment',
        {key: 'a', keyCode: 65, device: 'usb'});

    // This should cause the dialog to close.
    await browserProxy.methodCalled(
        'notifySwitchAccessActionAssignmentDialogDetached');
  });

  test('Switch access action assignment dialog error state', async function() {
    initPage();

    // Simulate a click on the select link row.
    page.$.selectLinkRow.click();

    await browserProxy.methodCalled(
        'notifySwitchAccessActionAssignmentDialogAttached');

    // Simulate pressing 'a', and then 'b'.
    cr.webUIListenerCallback(
        'switch-access-got-key-press-for-assignment',
        {key: 'a', keyCode: 65, device: 'usb'});
    cr.webUIListenerCallback(
        'switch-access-got-key-press-for-assignment',
        {key: 'b', keyCode: 66, device: 'usb'});

    const element = page.$$('#switchAccessActionAssignmentDialog');
    await test_util.waitAfterNextRender(element);

    // This should update the error field at the bottom of the dialog.
    const errorText = page.$$('#switchAccessActionAssignmentDialog')
                          .$$('#error')
                          .textContent.trim();
    assertEquals('Keys do not match. Press any key to exit.', errorText);
  });

  test('Deep link to auto-scan keyboards', async () => {
    loadTimeData.overrideValues({
      isDeepLinkingEnabled: true,
      showExperimentalAccessibilitySwitchAccessImprovedTextInput: true,
    });
    const prefs = getDefaultPrefs();
    prefs.settings.a11y.switch_access.auto_scan.enabled.value = true;
    initPage(prefs);

    Polymer.dom.flush();

    const params = new URLSearchParams;
    params.append('settingId', '1525');
    settings.Router.getInstance().navigateTo(
        settings.routes.MANAGE_SWITCH_ACCESS_SETTINGS, params);

    const deepLinkElement = page.$$('#keyboardScanSpeedSlider').$$('cr-slider');
    await test_util.waitAfterNextRender(deepLinkElement);

    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Auto-scan keyboard toggle should be focused for settingId=1525.');
  });
});
