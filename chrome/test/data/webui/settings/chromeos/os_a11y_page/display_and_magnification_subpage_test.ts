// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {SettingsDisplayAndMagnificationSubpageElement} from 'chrome://os-settings/lazy_load.js';
import {CrSettingsPrefs, Router, routes, SettingsDropdownMenuElement, SettingsPrefsElement, SettingsSliderElement, SettingsToggleButtonElement} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {pressAndReleaseKeyOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertGT, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitBeforeNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

suite('<settings-display-and-magnification-subpage>', () => {
  let page: SettingsDisplayAndMagnificationSubpageElement;
  let prefElement: SettingsPrefsElement;

  async function initPage() {
    prefElement = document.createElement('settings-prefs');
    document.body.appendChild(prefElement);

    await CrSettingsPrefs.initialized;
    page = document.createElement('settings-display-and-magnification-subpage');
    page.prefs = prefElement.prefs;
    document.body.appendChild(page);
    flush();
  }

  setup(() => {
    loadTimeData.overrideValues(
        {isAccessibilityOSSettingsVisibilityEnabled: true});
    Router.getInstance().navigateTo(routes.A11Y_DISPLAY_AND_MAGNIFICATION);
  });

  teardown(() => {
    page.remove();
    prefElement.remove();
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

          const subpageButton =
              page.shadowRoot!.querySelector<HTMLElement>(selector);
          assert(subpageButton);

          subpageButton.click();
          assertEquals(route, router.currentRoute);
          assertNotEquals(
              subpageButton, page.shadowRoot!.activeElement,
              `${selector} should not be focused`);

          const popStateEventPromise = eventToPromise('popstate', window);
          router.navigateToPreviousRoute();
          await popStateEventPromise;
          await waitBeforeNextRender(page);

          assertEquals(
              routes.A11Y_DISPLAY_AND_MAGNIFICATION, router.currentRoute);
          assertEquals(
              subpageButton, page.shadowRoot!.activeElement,
              `${selector} should be focused`);
        });
  });

  test('no subpages are available in kiosk mode', async () => {
    loadTimeData.overrideValues({
      isKioskModeActive: true,
      showTabletModeShelfNavigationButtonsSettings: true,
    });
    await initPage();

    const subpageLinks = page.shadowRoot!.querySelectorAll('cr-link-row');
    subpageLinks.forEach(subpageLink => assertFalse(isVisible(subpageLink)));
  });

  test('Turns on color enhancement filters', async () => {
    // Enabled in os_settings_v3_browsertest.js.
    assertTrue(loadTimeData.getBoolean(
        'areExperimentalAccessibilityColorEnhancementSettingsEnabled'));
    await initPage();

    assertFalse(page.prefs.settings.a11y.color_filtering.enabled.value);

    let colorDeficiencyDropdown =
        page.shadowRoot!.querySelector<SettingsDropdownMenuElement>(
            '#colorFilteringDeficiencyTypeDropdown');
    assertEquals(null, colorDeficiencyDropdown);

    let colorFilteringIntensitySlider =
        page.shadowRoot!.querySelector('colorFilteringIntensitySlider');
    assertEquals(null, colorFilteringIntensitySlider);

    const enableColorFilteringToggle =
        page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#enableColorFiltering');
    assert(enableColorFilteringToggle);
    assertTrue(isVisible(enableColorFilteringToggle));

    enableColorFilteringToggle.click();
    await waitBeforeNextRender(page);
    flush();

    assertTrue(page.prefs.settings.a11y.color_filtering.enabled.value);

    // Color enhancement options options become visible.
    colorDeficiencyDropdown =
        page.shadowRoot!.querySelector<SettingsDropdownMenuElement>(
            '#colorFilteringDeficiencyTypeDropdown');
    assert(colorDeficiencyDropdown);
    assertTrue(isVisible(colorDeficiencyDropdown));

    colorFilteringIntensitySlider =
        page.shadowRoot!.querySelector<SettingsSliderElement>(
            '#colorFilteringIntensitySlider');
    assert(colorFilteringIntensitySlider);
    assertTrue(isVisible(colorFilteringIntensitySlider));

    const amount = page.prefs.settings.a11y.color_filtering
                       .color_vision_correction_amount.value;

    // Try a keypress on the slider to ensure that it behaves OK.
    pressAndReleaseKeyOn(
        colorFilteringIntensitySlider.shadowRoot!.querySelector('cr-slider')!,
        37, [], 'ArrowLeft');
    await flushTasks();

    // The slider should have decreased, causing the pref to decrease.
    assertGT(
        amount,
        page.prefs.settings.a11y.color_filtering.color_vision_correction_amount
            .value);
  });
});
