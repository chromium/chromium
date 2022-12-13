// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/chromeos/lazy_load.js';

import {DevicePageBrowserProxyImpl, Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
import {webUIListenerCallback} from 'chrome://resources/ash/common/cr.m.js';
import {getDeepActiveElement} from 'chrome://resources/ash/common/util.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender, waitBeforeNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {TestDevicePageBrowserProxy} from './test_device_page_browser_proxy.js';

suite('ManageAccessibilityPageTests', function() {
  let page = null;
  let deviceBrowserProxy = null;

  function initPage(opt_prefs) {
    page = document.createElement('settings-manage-a11y-page');
    page.prefs = opt_prefs || getDefaultPrefs();
    document.body.appendChild(page);
  }

  function getDefaultPrefs() {
    return {
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
  }

  setup(function() {
    deviceBrowserProxy = new TestDevicePageBrowserProxy();
    deviceBrowserProxy.hasMouse = true;
    deviceBrowserProxy.hasTouchpad = true;
    deviceBrowserProxy.hasPointingStick = false;
    DevicePageBrowserProxyImpl.setInstanceForTesting(deviceBrowserProxy);

    loadTimeData.overrideValues({isKioskModeActive: true});
    PolymerTest.clearBody();
    Router.getInstance().navigateTo(routes.MANAGE_ACCESSIBILITY);
  });

  teardown(function() {
    if (page) {
      page.remove();
    }
    Router.getInstance().resetRouteForTesting();
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
        isVisible(page.shadowRoot.querySelector('setings-localized-link')));

    // Shelf navigation buttons are not shown in kiosk mode, even if
    // showTabletModeShelfNavigationButtonsSettings is true.
    assertFalse(isVisible(page.shadowRoot.querySelector(
        '#shelfNavigationButtonsEnabledControl')));

    const allowed_subpages = [
      'chromeVoxSubpageButton',
      'selectToSpeakSubpageButton',
      'ttsSubpageButton',
    ];

    const subpages = page.root.querySelectorAll('cr-link-row');
    subpages.forEach(function(subpage) {
      if (isVisible(subpage)) {
        assertTrue(allowed_subpages.includes(subpage.id));
      }
    });

    // Additional features link is not visible.
    assertFalse(isVisible(page.$.additionalFeaturesLink));
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
    const dictationSetting = page.shadowRoot.querySelector('#enableDictation');
    assertTrue(!!dictationSetting);
    assertTrue(dictationSetting.checked);
    assertEquals('Dictation', dictationSetting.label);
    assertEquals(
        'Type with your voice. Use Search + D, then start speaking.',
        dictationSetting.subLabel);

    // Dictation locale menu.
    const dictationLocaleMenuLabel =
        page.shadowRoot.querySelector('#dictationLocaleMenuLabel');
    const dictationLocaleMenuSubtitle =
        page.shadowRoot.querySelector('#dictationLocaleMenuSubtitle');
    assertTrue(!!dictationLocaleMenuLabel);
    assertTrue(!!dictationLocaleMenuSubtitle);
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
    page.dictationLocaleSubtitleOverride_ = 'Testing';
    flush();
    assertEquals(
        'English (United States) is processed locally and works offline',
        page.computeDictationLocaleSubtitle_());

    // Changing the Dictation locale pref should change the subtitle
    // computation.
    page.prefs.settings.a11y.dictation_locale.value = 'es';
    assertEquals(
        'Couldn’t download Spanish speech files. Download will be attempted ' +
            'later. Speech is sent to Google for processing until download ' +
            'is completed.',
        page.computeDictationLocaleSubtitle_());

    page.prefs.settings.a11y.dictation_locale.value = 'de';
    assertEquals(
        'German speech is sent to Google for processing',
        page.computeDictationLocaleSubtitle_());

    page.prefs.settings.a11y.dictation_locale.value = 'fr-FR';
    assertEquals(
        'French (France) speech is sent to Google for processing',
        page.computeDictationLocaleSubtitle_());

    // Only use the subtitle override once.
    page.useDictationLocaleSubtitleOverride_ = true;
    assertEquals('Testing', page.computeDictationLocaleSubtitle_());
    assertFalse(page.useDictationLocaleSubtitleOverride_);
    assertEquals(
        'French (France) speech is sent to Google for processing',
        page.computeDictationLocaleSubtitle_());
  });
});
