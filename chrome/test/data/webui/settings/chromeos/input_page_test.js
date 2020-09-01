// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {LanguagesBrowserProxyImpl, LanguagesMetricsProxyImpl} from 'chrome://os-settings/chromeos/lazy_load.js';
// #import {CrSettingsPrefs, Router} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {getFakeLanguagePrefs} from '../fake_language_settings_private.m.js'
// #import {FakeSettingsPrivate} from '../fake_settings_private.m.js';
// #import {TestLanguagesBrowserProxy} from './test_os_languages_browser_proxy.m.js';
// #import {TestLanguagesMetricsProxy} from './test_os_languages_metrics_proxy.m.js';
// #import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
// #import {fakeDataBind} from '../../test_util.m.js';
// clang-format on

suite('input page', () => {
  /** @type {!SettingsInputPageElement} */
  let inputPage;
  /** @type {!settings.LanguagesMetricsProxy} */
  let metricsProxy;

  suiteSetup(() => {
    CrSettingsPrefs.deferInitialization = true;
    loadTimeData.overrideValues({imeOptionsInSettings: true});
  });

  setup(() => {
    document.body.innerHTML = '';
    const prefElement = document.createElement('settings-prefs');
    const settingsPrivate =
        new settings.FakeSettingsPrivate(settings.getFakeLanguagePrefs());
    prefElement.initialize(settingsPrivate);
    document.body.appendChild(prefElement);

    return CrSettingsPrefs.initialized.then(() => {
      // Set up test browser proxy.
      const browserProxy = new settings.TestLanguagesBrowserProxy();
      settings.LanguagesBrowserProxyImpl.instance_ = browserProxy;

      // Sets up test metrics proxy.
      metricsProxy = new settings.TestLanguagesMetricsProxy();
      settings.LanguagesMetricsProxyImpl.instance_ = metricsProxy;

      // Set up fake languageSettingsPrivate API.
      const languageSettingsPrivate = browserProxy.getLanguageSettingsPrivate();
      languageSettingsPrivate.setSettingsPrefs(prefElement);

      // Instantiate the data model with data bindings for prefs.
      const settingsLanguages = document.createElement('settings-languages');
      settingsLanguages.prefs = prefElement.prefs;
      test_util.fakeDataBind(prefElement, settingsLanguages, 'prefs');
      document.body.appendChild(settingsLanguages);

      // Create page with data bindings for prefs and data model.
      inputPage = document.createElement('os-settings-input-page');
      inputPage.prefs = prefElement.prefs;
      test_util.fakeDataBind(prefElement, inputPage, 'prefs');
      inputPage.languages = settingsLanguages.languages;
      test_util.fakeDataBind(settingsLanguages, inputPage, 'languages');
      inputPage.languageHelper = settingsLanguages.languageHelper;
      test_util.fakeDataBind(settingsLanguages, inputPage, 'language-helper');
      document.body.appendChild(inputPage);
    });
  });

  suite('input method list', () => {
    test('displays correctly', () => {
      const inputMethodsList = inputPage.$$('#inputMethodsList');
      assertTrue(!!inputMethodsList);

      // The test input methods should appear.
      const items = inputMethodsList.querySelectorAll('.list-item');
      // Two items for input methods and one item for add input methods.
      assertEquals(3, items.length);
      assertEquals(
          'US keyboard',
          items[0].querySelector('.display-name').textContent.trim());
      assertTrue(!!items[0].querySelector('.internal-wrapper'));
      assertFalse(!!items[0].querySelector('.external-wrapper'));
      assertEquals(
          'US Dvorak keyboard',
          items[1].querySelector('.display-name').textContent.trim());
      assertTrue(!!items[1].querySelector('.external-wrapper'));
      assertFalse(!!items[1].querySelector('.internal-wrapper'));
    });

    test('navigates to input method options page', () => {
      const inputMethodsList = inputPage.$.inputMethodsList;
      const items = inputMethodsList.querySelectorAll('.list-item');
      items[0].querySelector('.subpage-arrow').click();
      const router = settings.Router.getInstance();
      assertEquals(
          router.getCurrentRoute().getAbsolutePath(),
          'chrome://os-settings/osLanguages/inputMethodOptions');
      assertEquals(
          router.getQueryParameters().get('id'),
          '_comp_ime_jkghodnilhceideoidjikpgommlajknkxkb:us::eng');
    });

    test('removes an input method', () => {
      const inputMethodName = 'US keyboard';

      let inputMethodsList = inputPage.$.inputMethodsList;
      let items = inputMethodsList.querySelectorAll('.list-item');
      assertEquals(3, items.length);
      assertEquals(
          inputMethodName,
          items[0].querySelector('.display-name').textContent.trim());
      assertFalse(!!inputPage.$$('os-settings-remove-input-method-dialog'));

      // opens the remove input method dialog.
      items[0].querySelector('.icon-clear').click();
      Polymer.dom.flush();

      const dialog = inputPage.$$('os-settings-remove-input-method-dialog');
      assertTrue(!!dialog);
      assertTrue(
          dialog.$$('[slot=title]').textContent.includes(inputMethodName));

      // removes the input method.
      const actionButton = dialog.$$('.action-button');
      assertTrue(!!actionButton);
      actionButton.click();
      Polymer.dom.flush();

      inputMethodsList = inputPage.$.inputMethodsList;
      items = inputMethodsList.querySelectorAll('.list-item');
      assertEquals(2, items.length);
      assertTrue(
          items[0].querySelector('.display-name').textContent.trim() !==
          inputMethodName);
    });
  });

  suite('add input methods dialog', () => {
    test('opens when clicking addInputMethod button', () => {
      assertFalse(!!inputPage.$$('os-settings-add-input-methods-dialog'));
      inputPage.$$('#addInputMethod').click();
      Polymer.dom.flush();
      assertTrue(!!inputPage.$$('os-settings-add-input-methods-dialog'));
    });
  });

  suite('records metrics', () => {
    test('when deactivating show ime menu', async () => {
      inputPage.setPrefValue('settings.language.ime_menu_activated', true);
      inputPage.$$('#showImeMenu').click();
      Polymer.dom.flush();

      assertFalse(
          await metricsProxy.whenCalled('recordToggleShowInputOptionsOnShelf'));
    });

    test('when activating show ime menu', async () => {
      inputPage.setPrefValue('settings.language.ime_menu_activated', false);
      inputPage.$$('#showImeMenu').click();
      Polymer.dom.flush();

      assertTrue(
          await metricsProxy.whenCalled('recordToggleShowInputOptionsOnShelf'));
    });

    test('when adding input methods', async () => {
      inputPage.$$('#addInputMethod').click();
      Polymer.dom.flush();

      await metricsProxy.whenCalled('recordAddInputMethod');
    });
  });
});
