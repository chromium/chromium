// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {SettingsKeyboardAndTextInputPageElement} from 'chrome://os-settings/lazy_load.js';
import {CrLinkRowElement, CrSettingsPrefs, Router, routes, SettingsPrefsElement, SettingsSliderElement, SettingsToggleButtonElement} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {pressAndReleaseKeyOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNotEquals, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

suite('<settings-keyboard-and-text-input-page>', () => {
  let page: SettingsKeyboardAndTextInputPageElement;
  let prefElement: SettingsPrefsElement;

  async function initPage() {
    prefElement = document.createElement('settings-prefs');
    document.body.appendChild(prefElement);
    await CrSettingsPrefs.initialized;

    page = document.createElement('settings-keyboard-and-text-input-page');
    page.prefs = prefElement.prefs;
    document.body.appendChild(page);
    flush();
  }

  setup(() => {
    Router.getInstance().navigateTo(routes.A11Y_KEYBOARD_AND_TEXT_INPUT);
  });

  teardown(() => {
    page.remove();
    prefElement.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('Dictation labels', async () => {
    // Ensure that the Dictation locale menu is shown by setting the dictation
    // pref to true (done in default prefs) and populating dictation locale
    // options with mock data.
    await initPage();

    page.setPrefValue('settings.a11y.dictation', true);
    page.setPrefValue('settings.a11y.dictation_locale', 'en-US');

    const locales = [{
      name: 'English (United States)',
      worksOffline: true,
      installed: true,
      recommended: true,
      value: 'en-US',
    }];
    webUIListenerCallback('dictation-locales-set', locales);
    flush();

    // Dictation toggle.
    const dictationSetting =
        page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#enableDictation');
    assert(dictationSetting);
    assertTrue(dictationSetting.checked);
    assertEquals('Dictation', dictationSetting.label);
    assertEquals(
        'Type with your voice. Use Search + D, then start speaking.',
        dictationSetting.subLabel);

    // Dictation locale menu.
    const dictationLocaleMenuLabel =
        page.shadowRoot!.querySelector<HTMLElement>(
            '#dictationLocaleMenuLabel');
    const dictationLocaleMenuSubtitle =
        page.shadowRoot!.querySelector<HTMLElement>(
            '#dictationLocaleMenuSubtitle');
    assert(dictationLocaleMenuLabel);
    assert(dictationLocaleMenuSubtitle);
    assertEquals('Language', dictationLocaleMenuLabel.innerText);
    assertEquals(
        'English (United States) is processed locally and works offline',
        dictationLocaleMenuSubtitle.innerText);

    // Fake a request to change the dictation locale menu subtitle.
    webUIListenerCallback('dictation-locale-menu-subtitle-changed', 'Testing');
    flush();

    // Only the dictation locale subtitle should have changed.
    assertEquals('Dictation', dictationSetting.label);
    assertEquals(
        'Type with your voice. Use Search + D, then start speaking.',
        dictationSetting.subLabel);
    assertEquals('Language', dictationLocaleMenuLabel.innerText);
    assertEquals('Testing', dictationLocaleMenuSubtitle.innerText);
  });

  test('Test computeDictationLocaleSubtitle_()', async () => {
    await initPage();
    const locales = [
      {
        name: 'English (United States)',
        worksOffline: true,
        installed: true,
        recommended: true,
        value: 'en-US',
      },
      {
        name: 'Spanish',
        worksOffline: true,
        installed: false,
        recommended: false,
        value: 'es',
      },
      {
        name: 'German',
        // Note: this data should never occur in practice. If a locale isn't
        // supported offline, then it should never be installed. Test this case
        // to verify our code still works given unexpected input.
        worksOffline: false,
        installed: true,
        recommended: false,
        value: 'de',
      },
      {
        name: 'French (France)',
        worksOffline: false,
        installed: false,
        recommended: false,
        value: 'fr-FR',
      },
    ];
    webUIListenerCallback('dictation-locales-set', locales);
    flush();

    page.setPrefValue('settings.a11y.dictation_locale', 'en-US');
    flush();
    assertEquals(
        'English (United States) is processed locally and works offline',
        page.get('dictationLocaleMenuSubtitle_'));

    // Changing the Dictation locale pref should change the subtitle
    // computation.
    page.setPrefValue('settings.a11y.dictation_locale', 'es');
    assertEquals(
        'Couldn’t download Spanish speech files. Download will be attempted ' +
            'later. Speech is sent to Google for processing until download ' +
            'is completed.',
        page.get('dictationLocaleMenuSubtitle_'));

    page.setPrefValue('settings.a11y.dictation_locale', 'de');
    assertEquals(
        'German speech is sent to Google for processing',
        page.get('dictationLocaleMenuSubtitle_'));


    // Only use the subtitle override once.
    page.set('useDictationLocaleSubtitleOverride_', true);
    page.set('dictationLocaleSubtitleOverride_', 'Testing');

    assertEquals('Testing', page.get('dictationLocaleMenuSubtitle_'));
    assertFalse(page.get('useDictationLocaleSubtitleOverride_'));
    page.setPrefValue('settings.a11y.dictation_locale', 'fr-FR');
    assertEquals(
        'French (France) speech is sent to Google for processing',
        page.get('dictationLocaleMenuSubtitle_'));
  });

  test('some parts are hidden in kiosk mode', async () => {
    loadTimeData.overrideValues({
      isKioskModeActive: true,
      showTabletModeShelfNavigationButtonsSettings: true,
    });
    await initPage();
    flush();

    const subpageLinks = page.root!.querySelectorAll('cr-link-row');
    subpageLinks.forEach(subpageLink => assertFalse(isVisible(subpageLink)));
  });

  test('Deep link to switch access', async () => {
    loadTimeData.overrideValues({
      isKioskModeActive: false,
    });
    await initPage();

    const params = new URLSearchParams();
    params.append('settingId', '1522');
    Router.getInstance().navigateTo(
        routes.A11Y_KEYBOARD_AND_TEXT_INPUT, params);

    flush();

    const deepLinkElement =
        page.shadowRoot!.querySelector('#enableSwitchAccess')!.shadowRoot!
            .querySelector('cr-toggle');
    assert(deepLinkElement);
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Switch access toggle should be focused for settingId=1522.');
  });

  test('Caret blink interval setting', async () => {
    await initPage();
    if (!loadTimeData.getBoolean(
            'isAccessibilityCaretBlinkIntervalSettingEnabled')) {
      // Caret blink interval section should not be visible if flag is disabled.
      const caretBlinkIntervalRow =
          page.shadowRoot!.querySelector('#caretBlinkIntervalRow');
      assertNull(caretBlinkIntervalRow);
      return;
    }

    // Caret blink interval section is visible. Test it is connected to
    // the expected preference.
    const caretBlinkIntervalRow =
        page.shadowRoot!.querySelector('#caretBlinkIntervalRow');
    assert(caretBlinkIntervalRow);
    assertTrue(isVisible(caretBlinkIntervalRow));

    const caretBlinkIntervalSlider =
        page.shadowRoot!.querySelector<SettingsSliderElement>(
            '#caretBlinkIntervalSlider');
    assert(caretBlinkIntervalSlider);
    assertTrue(isVisible(caretBlinkIntervalSlider));

    // Check the default synthetic pref value is what is expected.
    assertEquals(100, caretBlinkIntervalSlider.pref.value);
    // Check the actual default interval is what is expected.
    assertEquals(500, page.prefs.settings.a11y.caret.blink_interval.value);

    const slider =
        caretBlinkIntervalSlider.shadowRoot!.querySelector('cr-slider');
    assert(slider);

    // Make the repeat interval shorter (increase the slider value).
    pressAndReleaseKeyOn(slider, 39, [], 'ArrowRight');
    await flushTasks();

    assertEquals(110, caretBlinkIntervalSlider.pref.value);
    assertEquals(455, page.prefs.settings.a11y.caret.blink_interval.value);

    pressAndReleaseKeyOn(slider, 39, [], 'ArrowRight');
    await flushTasks();

    assertEquals(120, caretBlinkIntervalSlider.pref.value);
    assertEquals(417, page.prefs.settings.a11y.caret.blink_interval.value);

    // Now use the left arrow to get the minimum value, which should be "don't
    // blink", aka 0.

    while (caretBlinkIntervalSlider.pref.value > 40) {
      pressAndReleaseKeyOn(slider, 37, [], 'ArrowLeft');
      await flushTasks();
    }

    assertEquals(40, caretBlinkIntervalSlider.pref.value);
    assertEquals(0, page.prefs.settings.a11y.caret.blink_interval.value);
  });

  const selectorRouteList = [
    {selector: '#keyboardSubpageButton', route: routes.KEYBOARD},
  ];

  selectorRouteList.forEach(({selector, route}) => {
    test(
        `should focus ${selector} button when returning from ${
            route.path} subpage`,
        async () => {
          await initPage();
          flush();
          const router = Router.getInstance();

          const subpageButton =
              page.shadowRoot!.querySelector<CrLinkRowElement>(selector);
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
              routes.A11Y_KEYBOARD_AND_TEXT_INPUT, router.currentRoute);
          assertEquals(
              subpageButton, page.shadowRoot!.activeElement,
              `${selector} should be focused`);
        });
  });

  const settingsToggleButtons = [
    {
      id: 'stickyKeysToggle',
      prefKey: 'settings.a11y.sticky_keys_enabled',
      cvoxTooltipId: 'stickyKeysDisabledTooltip',
    },
    {
      id: 'focusHighlightToggle',
      prefKey: 'settings.a11y.focus_highlight',
      cvoxTooltipId: 'focusHighlightDisabledTooltip',
    },
    {
      id: 'caretHighlightToggle',
      prefKey: 'settings.a11y.caret_highlight',
      cvoxTooltipId: '',
    },
    {
      id: 'caretBrowsingToggle',
      prefKey: 'settings.a11y.caretbrowsing.enabled',
      cvoxTooltipId: '',
    },
  ];

  settingsToggleButtons.forEach(({id, prefKey, cvoxTooltipId}) => {
    test(`Accessibility toggle button syncs to prefs: ${id}`, async () => {
      await initPage();
      // Find the toggle and ensure that it's:
      // 1. Not checked
      // 2. The associated pref is off
      const toggle =
          page.shadowRoot!.querySelector<SettingsToggleButtonElement>(`#${id}`);
      assert(toggle);
      assertFalse(toggle.checked);
      let pref = page.getPref(prefKey);
      assertFalse(pref.value);

      // Click the toggle. Ensure that it's:
      // 1. Checked
      // 2. The associated pref is on
      toggle.click();
      assertTrue(toggle.checked);
      pref = page.getPref(prefKey);
      assertTrue(pref.value);

      if (cvoxTooltipId === '') {
        return;
      }

      const disabledTooltipIcon =
          page.shadowRoot!.querySelector(`#${cvoxTooltipId}`);
      assert(disabledTooltipIcon);
      assertFalse(isVisible(disabledTooltipIcon));

      // Turn on ChromeVox.
      page.setPrefValue('settings.accessibility', true);
      assertTrue(toggle.disabled);
      assertTrue(isVisible(disabledTooltipIcon));
      assertFalse(toggle.checked);

      // Turn off ChromeVox again.
      page.setPrefValue('settings.accessibility', false);
      assertFalse(toggle.disabled);
      assertFalse(isVisible(disabledTooltipIcon));
      assertTrue(toggle.checked);
    });
  });
});
