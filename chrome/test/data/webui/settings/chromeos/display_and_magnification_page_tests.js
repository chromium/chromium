// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/chromeos/lazy_load.js';

import {Router, routes, SettingsDropdownMenuElement, SettingsSliderElement, SettingsToggleButtonElement} from 'chrome://os-settings/chromeos/os_settings.js';
import {pressAndReleaseKeyOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertGT, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitBeforeNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';


suite('DisplayAndMagnificationPageTests', function() {
  let page = null;

  async function initPage() {
    page = document.createElement('settings-display-and-magnification-page');
    page.prefs = {
      settings: {
        a11y: {
          color_filtering: {
            enabled: {
              key: 'settings.a11y.color_filtering.enabled',
              type: chrome.settingsPrivate.PrefType.BOOLEAN,
              value: false,
            },
            color_vision_correction_amount: {
              key:
                  'settings.a11y.color_filtering.color_vision_correction_amount',
              type: chrome.settingsPrivate.PrefType.NUMBER,
              value: 100,
            },
          },
        },
      },
    };
    document.body.appendChild(page);
    flush();
  }

  setup(function() {
    PolymerTest.clearBody();
    loadTimeData.overrideValues(
        {isAccessibilityOSSettingsVisibilityEnabled: true});
    Router.getInstance().navigateTo(routes.A11Y_DISPLAY_AND_MAGNIFICATION);
  });

  teardown(function() {
    if (page) {
      page.remove();
    }
    Router.getInstance().resetRouteForTesting();
  });

  [{selector: '#displaySubpageButton', route: routes.DISPLAY},
  ].forEach(({selector, route}) => {
    test(
        `should focus ${selector} button when returning from ${
            route.path} subpage`,
        async () => {
          await initPage();
          const router = Router.getInstance();

          const subpageButton = page.shadowRoot.querySelector(selector);
          assertTrue(!!subpageButton);

          subpageButton.click();
          assertEquals(route, router.currentRoute);
          assertNotEquals(
              subpageButton, page.shadowRoot.activeElement,
              `${selector} should not be focused`);

          const popStateEventPromise = eventToPromise('popstate', window);
          router.navigateToPreviousRoute();
          await popStateEventPromise;
          await waitBeforeNextRender(page);

          assertEquals(
              routes.A11Y_DISPLAY_AND_MAGNIFICATION, router.currentRoute);
          assertEquals(
              subpageButton, page.shadowRoot.activeElement,
              `${selector} should be focused`);
        });
  });

  test('no subpages are available in kiosk mode', async function() {
    loadTimeData.overrideValues({
      isKioskModeActive: true,
      showTabletModeShelfNavigationButtonsSettings: true,
    });
    await initPage();

    const subpageLinks = page.root.querySelectorAll('cr-link-row');
    subpageLinks.forEach(subpageLink => assertFalse(isVisible(subpageLink)));
  });

  test('Turns on color enhancement filters', async () => {
    // Enabled in os_settings_v3_browsertest.js.
    assertTrue(loadTimeData.getBoolean(
        'areExperimentalAccessibilityColorEnhancementSettingsEnabled'));
    await initPage();

    assertFalse(page.prefs.settings.a11y.color_filtering.enabled.value);

    let colorDeficiencyDropdown =
        page.shadowRoot.querySelector('#colorFilteringDeficiencyTypeDropdown');
    assertEquals(null, colorDeficiencyDropdown);

    let colorFilteringIntensitySlider =
        page.shadowRoot.querySelector('#colorFilteringIntensitySlider');
    assertEquals(null, colorFilteringIntensitySlider);

    const enableColorFilteringToggle =
        page.shadowRoot.querySelector('#enableColorFiltering');
    assertTrue(!!enableColorFilteringToggle);
    assertTrue(isVisible(enableColorFilteringToggle));

    enableColorFilteringToggle.click();
    await waitBeforeNextRender(page);
    flush();

    assertTrue(page.prefs.settings.a11y.color_filtering.enabled.value);

    // Color enhancement options options become visible.
    colorDeficiencyDropdown =
        page.shadowRoot.querySelector('#colorFilteringDeficiencyTypeDropdown');
    assertTrue(!!colorDeficiencyDropdown);
    assertTrue(isVisible(colorDeficiencyDropdown));

    colorFilteringIntensitySlider =
        page.shadowRoot.querySelector('#colorFilteringIntensitySlider');
    assertTrue(!!colorFilteringIntensitySlider);
    assertTrue(isVisible(colorFilteringIntensitySlider));

    const amount = page.prefs.settings.a11y.color_filtering
                       .color_vision_correction_amount.value;

    // Try a keypress on the slider to ensure that it behaves OK.
    pressAndReleaseKeyOn(
        colorFilteringIntensitySlider.shadowRoot.querySelector('cr-slider'), 37,
        [], 'ArrowLeft');
    await flushTasks();

    // The slider should have decreased, causing the pref to decrease.
    assertGT(
        amount,
        page.prefs.settings.a11y.color_filtering.color_vision_correction_amount
            .value);
  });
});
