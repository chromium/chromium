// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {SettingsManageA11ySubpageElement} from 'chrome://os-settings/lazy_load.js';
import {DevicePageBrowserProxyImpl, Router, routes, SettingsToggleButtonElement} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestDevicePageBrowserProxy} from '../device_page/test_device_page_browser_proxy.js';

suite('<settings-manage-a11y-subpage>', () => {
  let page: SettingsManageA11ySubpageElement;
  let deviceBrowserProxy: TestDevicePageBrowserProxy;
  const defaultPrefs = {
    'settings': {
      'a11y': {
        'tablet_mode_shelf_nav_buttons_enabled': {
          key: 'settings.a11y.tablet_mode_shelf_nav_buttons_enabled',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        },
        'dictation': {
          key: 'prefs.settings.a11y.dictation',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: true,
        },
        'dictation_locale': {
          key: 'prefs.settings.a11y.dictation_locale',
          type: chrome.settingsPrivate.PrefType.STRING,
          value: 'en-US',
        },
      },
      'accessibility': {
        key: 'settings.accessibility',
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: false,
      },
    },
  };

  function initPage() {
    page = document.createElement('settings-manage-a11y-subpage');
    page.prefs = defaultPrefs;
    document.body.appendChild(page);
  }

  setup(function() {
    deviceBrowserProxy = new TestDevicePageBrowserProxy();
    deviceBrowserProxy.hasMouse = true;
    deviceBrowserProxy.hasTouchpad = true;
    deviceBrowserProxy.hasPointingStick = false;
    DevicePageBrowserProxyImpl.setInstanceForTesting(deviceBrowserProxy);

    loadTimeData.overrideValues({isKioskModeActive: true});
    Router.getInstance().navigateTo(routes.MANAGE_ACCESSIBILITY);
  });

  teardown(function() {
    page.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test(
      'Page loads without crashing when prefs are not yet initialized in kiosk mode',
      () => {
        loadTimeData.overrideValues({isKioskModeActive: true});
        page = document.createElement('settings-manage-a11y-subpage');
        document.body.appendChild(page);

        // Intentionally set prefs after page is appended to DOM to simulate
        // asynchronicity of initializing prefs
        page.prefs = defaultPrefs;
      });

  test('some parts are hidden in kiosk mode', function() {
    loadTimeData.overrideValues({
      isKioskModeActive: true,
      showTabletModeShelfNavigationButtonsSettings: true,
    });
    initPage();
    // Add mouse and touchpad to show some hidden settings.
    deviceBrowserProxy.hasMouse = true;
    deviceBrowserProxy.hasTouchpad = true;
    flush();

    // Accessibility learn more link should be hidden.
    assertFalse(
        isVisible(page.shadowRoot!.querySelector('setings-localized-link')));

    // Shelf navigation buttons are not shown in kiosk mode, even if
    // showTabletModeShelfNavigationButtonsSettings is true.
    assertFalse(isVisible(page.shadowRoot!.querySelector(
        '#shelfNavigationButtonsEnabledControl')));

    const allowed_subpages = [
      'chromeVoxSubpageButton',
      'selectToSpeakSubpageButton',
      'ttsSubpageButton',
    ];

    const subpages = page.root!.querySelectorAll('cr-link-row');
    subpages.forEach(function(subpage) {
      if (isVisible(subpage)) {
        assertTrue(allowed_subpages.includes(subpage.id));
      }
    });

    // Additional features link is not visible.
    assertFalse(
        isVisible(page.shadowRoot!.querySelector('#additionalFeaturesLink')));
  });

  test('Dictation labels', async () => {
    // Ensure that the Dictation locale menu is shown by setting the dictation
    // pref to true (done in default prefs) and populating dictation locale
    // options with mock data.
    initPage();
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
    initPage();
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
    page.set('dictationLocaleSubtitleOverride_', 'Testing');
    flush();
    assertEquals(
        'English (United States) is processed locally and works offline',
        page.get('dictationLocaleMenuSubtitle_'));

    // Changing the Dictation locale pref should change the subtitle
    // computation.
    page.set('prefs.settings.a11y.dictation_locale.value', 'es');
    assertEquals(
        'Couldn’t download Spanish speech files. Download will be attempted ' +
            'later. Speech is sent to Google for processing until download ' +
            'is completed.',
        page.get('dictationLocaleMenuSubtitle_'));

    page.set('prefs.settings.a11y.dictation_locale.value', 'de');
    assertEquals(
        'German speech is sent to Google for processing',
        page.get('dictationLocaleMenuSubtitle_'));

    page.set('prefs.settings.a11y.dictation_locale.value', 'fr-FR');
    assertEquals(
        'French (France) speech is sent to Google for processing',
        page.get('dictationLocaleMenuSubtitle_'));

    // Only use the subtitle override once.
    page.set('useDictationLocaleSubtitleOverride_', true);
    assertEquals('Testing', page['computeDictationLocaleSubtitle_']());
    assertFalse(page.get('useDictationLocaleSubtitleOverride_'));
    assertEquals(
        'French (France) speech is sent to Google for processing',
        page['computeDictationLocaleSubtitle_']());
  });
});
