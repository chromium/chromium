// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

  test('Switch assignment key display', function() {
    initPage();

    assertEquals(0, page.selectAssignments_.length);
    assertEquals(0, page.nextAssignments_.length);
    assertEquals(0, page.previousAssignments_.length);

    // Simulate a pref change for the select action.
    cr.webUIListenerCallback(
        'switch-access-assignments-changed',
        {select: ['a'], next: [], previous: []});

    assertEquals(1, page.selectAssignments_.length);
    assertEquals('a', page.selectAssignments_[0]);
    assertEquals(0, page.nextAssignments_.length);
    assertEquals(0, page.previousAssignments_.length);
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
        'switch-access-got-key-press-for-assignment', {key: 'a', keyCode: 65});
    cr.webUIListenerCallback(
        'switch-access-got-key-press-for-assignment', {key: 'a', keyCode: 65});

    // This should cause the dialog to close.
    await browserProxy.methodCalled(
        'notifySwitchAccessActionAssignmentDialogDetached');
  });

  test('Deep link to auto-scan keyboards', async () => {
    loadTimeData.overrideValues({
      isDeepLinkingEnabled: true,
      showExperimentalAccessibilitySwitchAccessImprovedTextInput: true,
    });
    prefs = getDefaultPrefs();
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
