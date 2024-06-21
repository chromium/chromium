// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {SettingsDisplayAndMagnificationSubpageElement} from 'chrome://os-settings/lazy_load.js';
import {CrSettingsPrefs, Router, routes, settingMojom, SettingsDropdownMenuElement, SettingsPrefsElement, SettingsSliderElement, SettingsToggleButtonElement} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {pressAndReleaseKeyOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertGT, assertNotEquals, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender, waitBeforeNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

const DEUTERANOMALY_VALUE = 1;
const GREYSCALE_VALUE = 3;

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
          await waitAfterNextRender(page);

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

    // Try changing the color filtering type.
    const filterSelectElement =
        colorDeficiencyDropdown.shadowRoot!.querySelector('select');
    assert(filterSelectElement);
    assertEquals(String(DEUTERANOMALY_VALUE), filterSelectElement.value);

    // Change the filtering type.
    filterSelectElement.value = String(GREYSCALE_VALUE);
    filterSelectElement.dispatchEvent(new CustomEvent('change'));
    const new_filter = page.prefs.settings.a11y.color_filtering
                           .color_vision_deficiency_type.value;
    assertEquals(new_filter, GREYSCALE_VALUE);
  });

  test('Turns on reduced animations', async () => {
    await initPage();

    if (loadTimeData.getBoolean('isAccessibilityReducedAnimationsEnabled')) {
      // If the flag is enabled, check that the UI works.
      assertFalse(page.prefs.settings.a11y.reduced_animations.enabled.value);

      const enableReducedAnimationsToggle =
          page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
              '#enableReducedAnimations');
      assert(enableReducedAnimationsToggle);
      assertTrue(isVisible(enableReducedAnimationsToggle));

      enableReducedAnimationsToggle.click();
      await waitBeforeNextRender(page);
      flush();

      assertTrue(page.prefs.settings.a11y.reduced_animations.enabled.value);
    } else {
      // Toggle shouldn't be available if flag is disabled.
      const enableReducedAnimationsToggle =
          page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
              '#enableReducedAnimations');
      assert(!enableReducedAnimationsToggle);
    }
  });

  if (loadTimeData.getBoolean(
          'isAccessibilityMagnifierFollowsChromeVoxEnabled')) {
    test('Turns off docked magnifier follows ChromeVox', async () => {
      await initPage();
      const dockedMagnifierToggle =
          page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
              '#dockedMagnifierToggle');

      assert(dockedMagnifierToggle);

      dockedMagnifierToggle.click();
      await waitBeforeNextRender(page);

      const dockedMagnifierFollowsChromeVoxToggle =
          page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
              '#dockedMagnifierFollowsChromeVoxToggle');

      assert(dockedMagnifierFollowsChromeVoxToggle);
      assertTrue(isVisible(dockedMagnifierFollowsChromeVoxToggle));
      // Docked magnifier follows ChromeVox toggle should be enabled by
      // default.
      assertTrue(page.prefs.settings.a11y
                     .screen_magnifier_chromevox_focus_following.value);

      dockedMagnifierFollowsChromeVoxToggle.click();
      await waitBeforeNextRender(page);
      flush();

      assertFalse(page.prefs.settings.a11y
                      .screen_magnifier_chromevox_focus_following.value);
    });
  } else {
    test(
        'Docked magnifier follows ChromeVox toggle does not appear',
        async () => {
          await initPage();
          const dockedMagnifierToggle =
              page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                  '#dockedMagnifierToggle');

          assert(dockedMagnifierToggle);

          dockedMagnifierToggle.click();
          await waitBeforeNextRender(page);

          // Toggle shouldn't be available if flag is disabled.
          const dockedMagnifierFollowsChromeVoxToggle =
              page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                  '#dockedMagnifierFollowsChromeVoxToggle');

          assertNull(dockedMagnifierFollowsChromeVoxToggle);
        });
  }

  if (loadTimeData.getBoolean(
          'isAccessibilityMagnifierFollowsChromeVoxEnabled')) {
    test('Turns off fullscreen magnifier follows ChromeVox', async () => {
      await initPage();

      const fullScreenMagnifierToggle =
          page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
              '#fullScreenMagnifierToggle');

      assert(fullScreenMagnifierToggle);

      fullScreenMagnifierToggle.click();
      await waitBeforeNextRender(page);

      const fullScreenMagnifierFollowsChromeVoxToggle =
          page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
              '#fullScreenMagnifierFollowsChromeVoxToggle');

      assert(fullScreenMagnifierFollowsChromeVoxToggle);
      assertTrue(isVisible(fullScreenMagnifierFollowsChromeVoxToggle));
      // Full Screen magnifier follows ChromeVox toggle should be enabled by
      // default.
      assertTrue(page.prefs.settings.a11y
                     .screen_magnifier_chromevox_focus_following.value);

      fullScreenMagnifierFollowsChromeVoxToggle.click();
      await waitBeforeNextRender(page);
      flush();

      assertFalse(page.prefs.settings.a11y
                      .screen_magnifier_chromevox_focus_following.value);
    });
  } else {
    test(
        'Fullscreen magnifier follows ChromeVox toggle does not appear',
        async () => {
          await initPage();
          const fullScreenMagnifierToggle =
              page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                  '#fullScreenMagnifierToggle');

          assert(fullScreenMagnifierToggle);

          fullScreenMagnifierToggle.click();
          await waitBeforeNextRender(page);

          // Toggle shouldn't be available if flag is disabled.
          const fullScreenMagnifierFollowsChromeVoxToggle =
              page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                  '#fullScreenMagnifierFollowsChromeVoxToggle');

          assertNull(fullScreenMagnifierFollowsChromeVoxToggle);
        });
  }

  if (loadTimeData.getBoolean(
          'isAccessibilityMagnifierFollowsChromeVoxEnabled')) {
    test(
        'kMagnifierFollowsChromeVox is deep-linked from fullscreen magnifier',
        async () => {
          await initPage();
          const setting = settingMojom.Setting.kMagnifierFollowsChromeVox;
          const params = new URLSearchParams();
          params.append('settingId', setting.toString());
          Router.getInstance().navigateTo(
              routes.A11Y_DISPLAY_AND_MAGNIFICATION, params);

          const fullScreenMagnifierToggle =
              page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                  '#fullScreenMagnifierToggle');

          assert(fullScreenMagnifierToggle);

          fullScreenMagnifierToggle.click();
          await waitBeforeNextRender(page);

          const deepLinkElement =
              page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                  '#fullScreenMagnifierFollowsChromeVoxToggle');

          assertTrue(!!deepLinkElement);

          await waitAfterNextRender(deepLinkElement);
          assertEquals(
              deepLinkElement, page.shadowRoot!.activeElement,
              `Element should be focused for settingId=${setting}.'`);
        });
  } else {
    test(
        'kMagnifierFollowsChromeVox not deep-linked from fullscreen magnifier',
        async () => {
          await initPage();

          const setting = settingMojom.Setting.kMagnifierFollowsChromeVox;
          const params = new URLSearchParams();
          params.append('settingId', setting.toString());
          Router.getInstance().navigateTo(
              routes.A11Y_DISPLAY_AND_MAGNIFICATION, params);

          const fullScreenMagnifierToggle =
              page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                  '#fullScreenMagnifierToggle');

          assert(fullScreenMagnifierToggle);

          fullScreenMagnifierToggle.click();
          await waitBeforeNextRender(page);

          const deepLinkElement =
              page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                  '#fullScreenMagnifierFollowsChromeVoxToggle');

          assertNull(deepLinkElement);
        });
  }

  if (loadTimeData.getBoolean(
          'isAccessibilityMagnifierFollowsChromeVoxEnabled')) {
    test(
        'kMagnifierFollowsChromeVox deep-linked from docked magnifier',
        async () => {
          await initPage();

          const setting = settingMojom.Setting.kMagnifierFollowsChromeVox;
          const params = new URLSearchParams();
          params.append('settingId', setting.toString());
          Router.getInstance().navigateTo(
              routes.A11Y_DISPLAY_AND_MAGNIFICATION, params);

          const dockedMagnifierToggle =
              page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                  '#dockedMagnifierToggle');

          assert(dockedMagnifierToggle);

          dockedMagnifierToggle.click();
          await waitBeforeNextRender(page);

          const deepLinkElement =
              page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                  '#dockedMagnifierFollowsChromeVoxToggle');

          assertTrue(!!deepLinkElement);

          await waitAfterNextRender(deepLinkElement);
          assertEquals(
              deepLinkElement, page.shadowRoot!.activeElement,
              `Element should be focused for settingId=${setting}.'`);
        });
  } else {
    test(
        'kMagnifierFollowsChromeVox is not deep-linked from docked magnifier',
        async () => {
          await initPage();

          const setting = settingMojom.Setting.kMagnifierFollowsChromeVox;
          const params = new URLSearchParams();
          params.append('settingId', setting.toString());
          Router.getInstance().navigateTo(
              routes.A11Y_DISPLAY_AND_MAGNIFICATION, params);

          const dockedMagnifierToggle =
              page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                  '#dockedMagnifierToggle');

          assert(dockedMagnifierToggle);

          dockedMagnifierToggle.click();
          await waitBeforeNextRender(page);

          const deepLinkElement =
              page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                  '#dockedMagnifierFollowsChromeVoxToggle');

          assertNull(deepLinkElement);
        });
  }

  if (loadTimeData.getBoolean('isAccessibilityMagnifierFollowsStsEnabled')) {
    test('Turns off docked magnifier follows select to speak', async () => {
      await initPage();
      const dockedMagnifierToggle =
          page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
              '#dockedMagnifierToggle');

      assert(dockedMagnifierToggle);

      dockedMagnifierToggle.click();
      await waitBeforeNextRender(page);

      const dockedMagnifierFollowsStsToggle =
          page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
              '#dockedMagnifierFollowsStsToggle');

      assert(dockedMagnifierFollowsStsToggle);
      assertTrue(isVisible(dockedMagnifierFollowsStsToggle));
      // Docked magnifier follows STS toggle should be enabled by default.
      assertTrue(page.prefs.settings.a11y
                     .screen_magnifier_select_to_speak_focus_following.value);

      dockedMagnifierFollowsStsToggle.click();
      await waitBeforeNextRender(page);
      flush();

      assertFalse(page.prefs.settings.a11y
                      .screen_magnifier_select_to_speak_focus_following.value);
    });
  } else {
    test(
        'Docked magnifier follows select to speak toggle does not appear',
        async () => {
          await initPage();
          const dockedMagnifierToggle =
              page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                  '#dockedMagnifierToggle');

          assert(dockedMagnifierToggle);

          dockedMagnifierToggle.click();
          await waitBeforeNextRender(page);

          // Toggle shouldn't be available if flag is disabled.
          const dockedMagnifierFollowsStsToggle =
              page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                  '#dockedMagnifierFollowsStsToggle');

          assertNull(dockedMagnifierFollowsStsToggle);
        });
  }

  if (loadTimeData.getBoolean('isAccessibilityMagnifierFollowsStsEnabled')) {
    test(
        'Turns off full screen magnifier follows select to speak', async () => {
          await initPage();

          const fullScreenMagnifierToggle =
              page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                  '#fullScreenMagnifierToggle');

          assert(fullScreenMagnifierToggle);

          fullScreenMagnifierToggle.click();
          await waitBeforeNextRender(page);

          const fullScreenMagnifierFollowsStsToggle =
              page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                  '#fullScreenMagnifierFollowsStsToggle');

          assert(fullScreenMagnifierFollowsStsToggle);
          assertTrue(isVisible(fullScreenMagnifierFollowsStsToggle));
          // Full Screen magnifier follows STS toggle should be enabled by
          // default.
          assertTrue(
              page.prefs.settings.a11y
                  .screen_magnifier_select_to_speak_focus_following.value);

          fullScreenMagnifierFollowsStsToggle.click();
          await waitBeforeNextRender(page);
          flush();

          assertFalse(
              page.prefs.settings.a11y
                  .screen_magnifier_select_to_speak_focus_following.value);
        });
  } else {
    test(
        'Full screen magnifier follows select to speak toggle does not appear',
        async () => {
          await initPage();
          const fullScreenMagnifierToggle =
              page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                  '#fullScreenMagnifierToggle');

          assert(fullScreenMagnifierToggle);

          fullScreenMagnifierToggle.click();
          await waitBeforeNextRender(page);

          // Toggle shouldn't be available if flag is disabled.
          const fullScreenMagnifierFollowsStsToggle =
              page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                  '#fullScreenMagnifierFollowsStsToggle');

          assertNull(fullScreenMagnifierFollowsStsToggle);
        });
  }

  if (loadTimeData.getBoolean('isAccessibilityMagnifierFollowsStsEnabled')) {
    test(
        'kAccessibilityMagnifierFollowsSts is deep-linked from full magnifier',
        async () => {
          await initPage();
          const setting =
              settingMojom.Setting.kAccessibilityMagnifierFollowsSts;
          const params = new URLSearchParams();
          params.append('settingId', setting.toString());
          Router.getInstance().navigateTo(
              routes.A11Y_DISPLAY_AND_MAGNIFICATION, params);

          const fullScreenMagnifierToggle =
              page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                  '#fullScreenMagnifierToggle');

          assert(fullScreenMagnifierToggle);

          fullScreenMagnifierToggle.click();
          await waitBeforeNextRender(page);

          const deepLinkElement =
              page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                  '#fullScreenMagnifierFollowsStsToggle');

          assertTrue(!!deepLinkElement);

          await waitAfterNextRender(deepLinkElement);
          assertEquals(
              deepLinkElement, page.shadowRoot!.activeElement,
              `Element should be focused for settingId=${setting}.'`);
        });
  } else {
    test(
        'kAccessibilityMagnifierFollowsSts not deep-linked from full magnifier',
        async () => {
          await initPage();

          const setting =
              settingMojom.Setting.kAccessibilityMagnifierFollowsSts;
          const params = new URLSearchParams();
          params.append('settingId', setting.toString());
          Router.getInstance().navigateTo(
              routes.A11Y_DISPLAY_AND_MAGNIFICATION, params);

          const fullScreenMagnifierToggle =
              page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                  '#fullScreenMagnifierToggle');

          assert(fullScreenMagnifierToggle);

          fullScreenMagnifierToggle.click();
          await waitBeforeNextRender(page);

          const deepLinkElement =
              page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                  '#fullScreenMagnifierFollowsStsToggle');

          assertNull(deepLinkElement);
        });
  }


  if (loadTimeData.getBoolean('isAccessibilityMagnifierFollowsStsEnabled')) {
    test(
        'kAccessibilityMagnifierFollowsSts deep-linked docked magnifier',
        async () => {
          await initPage();

          const setting =
              settingMojom.Setting.kAccessibilityMagnifierFollowsSts;
          const params = new URLSearchParams();
          params.append('settingId', setting.toString());
          Router.getInstance().navigateTo(
              routes.A11Y_DISPLAY_AND_MAGNIFICATION, params);

          const dockedMagnifierToggle =
              page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                  '#dockedMagnifierToggle');

          assert(dockedMagnifierToggle);

          dockedMagnifierToggle.click();
          await waitBeforeNextRender(page);

          const deepLinkElement =
              page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                  '#dockedMagnifierFollowsStsToggle');

          assertTrue(!!deepLinkElement);

          await waitAfterNextRender(deepLinkElement);
          assertEquals(
              deepLinkElement, page.shadowRoot!.activeElement,
              `Element should be focused for settingId=${setting}.'`);
        });
  } else {
    test(
        'kAccessibilityMagnifierFollowsSts is not deep-linked docked magnifier',
        async () => {
          await initPage();

          const setting =
              settingMojom.Setting.kAccessibilityMagnifierFollowsSts;
          const params = new URLSearchParams();
          params.append('settingId', setting.toString());
          Router.getInstance().navigateTo(
              routes.A11Y_DISPLAY_AND_MAGNIFICATION, params);

          const dockedMagnifierToggle =
              page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                  '#dockedMagnifierToggle');

          assert(dockedMagnifierToggle);

          dockedMagnifierToggle.click();
          await waitBeforeNextRender(page);

          const deepLinkElement =
              page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                  '#dockedMagnifierFollowsStsToggle');

          assertNull(deepLinkElement);
        });
  }
});
