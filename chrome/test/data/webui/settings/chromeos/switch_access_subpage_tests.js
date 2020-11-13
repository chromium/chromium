// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @param {!Array<{value:number, name:string}>} options
 * @param {number} value
 * @returns {boolean}
 */
function hasOptionWithValue(options, value) {
  return options.filter(o => o.value === value).length > 0;
}

suite('ManageAccessibilityPageTests', function() {
  let page = null;

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
    PolymerTest.clearBody();
  });

  teardown(function() {
    if (page) {
      page.remove();
    }
    settings.Router.getInstance().resetRouteForTesting();
  });

  test(
      'Switch assignment option unavailable when assigned to another command',
      function() {
        initPage();
        const assignedValue = SwitchAccessAssignmentValue.ONE;

        // Check that the value is available in all three dropdowns.
        assertTrue(hasOptionWithValue(page.optionsForNext_, assignedValue));
        assertTrue(hasOptionWithValue(page.optionsForPrevious_, assignedValue));
        assertTrue(hasOptionWithValue(page.optionsForSelect_, assignedValue));

        // Assign the value to one setting (next).
        page.prefs.settings.a11y.switch_access.next.setting.value =
            assignedValue;
        page.updateOptionsForDropdowns_();

        // Check that the value no longer appears in the other two dropdowns
        assertTrue(hasOptionWithValue(page.optionsForNext_, assignedValue));
        assertFalse(
            hasOptionWithValue(page.optionsForPrevious_, assignedValue));
        assertFalse(hasOptionWithValue(page.optionsForSelect_, assignedValue));
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
