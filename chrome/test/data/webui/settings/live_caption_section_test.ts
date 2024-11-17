// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {LanguageHelper, SettingsAddLanguagesDialogElement, SettingsLiveCaptionElement} from 'chrome://settings/lazy_load.js';
import {CaptionsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs, loadTimeData} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertStringContains, assertStringExcludes, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {fakeDataBind} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {TestCaptionsBrowserProxy} from './test_captions_browser_proxy.js';

suite('LiveCaptionSection', function() {
  let languageHelper: LanguageHelper;
  let liveCaptionSection: SettingsLiveCaptionElement;
  let browserProxy: TestCaptionsBrowserProxy;
  let dialog: SettingsAddLanguagesDialogElement|null = null;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      enableLiveCaptionMultiLanguage: true,
      enableLiveTranslate: true,
    });
  });

  setup(async () => {
    const settingsPrefs = document.createElement('settings-prefs');
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    const settingsLanguages = document.createElement('settings-languages');
    settingsLanguages.prefs = settingsPrefs.prefs;
    fakeDataBind(settingsPrefs, settingsLanguages, 'prefs');
    document.body.appendChild(settingsLanguages);

    document.body.appendChild(settingsPrefs);
    await CrSettingsPrefs.initialized;

    // Set up test browser proxy.
    browserProxy = new TestCaptionsBrowserProxy();
    CaptionsBrowserProxyImpl.setInstance(browserProxy);

    liveCaptionSection = document.createElement('settings-live-caption');
    liveCaptionSection.prefs = settingsPrefs.prefs;
    fakeDataBind(settingsPrefs, liveCaptionSection, 'prefs');
    liveCaptionSection.languageHelper = settingsLanguages.languageHelper;
    fakeDataBind(settingsLanguages, liveCaptionSection, 'language-helper');
    document.body.appendChild(liveCaptionSection);

    flush();
    languageHelper = liveCaptionSection.languageHelper;
    return languageHelper.whenReady();
  });

  test('test caption.enable toggle', function() {
    const settingsToggle =
        liveCaptionSection.shadowRoot!.querySelector<HTMLElement>(
            '#liveCaptionToggleButton');
    assertTrue(!!settingsToggle);

    // Clicking on the toggle switches it to true.
    settingsToggle.click();
    let newToggleValue =
        liveCaptionSection
            .getPref('accessibility.captions.live_caption_enabled')
            .value;
    assertTrue(newToggleValue);

    // Clicking on the toggle switches it to false.
    settingsToggle.click();
    newToggleValue = liveCaptionSection
                         .getPref('accessibility.captions.live_caption_enabled')
                         .value;
    assertFalse(newToggleValue);
  });

  test('add languages and confirm', async function() {
    const addLanguagesButton =
        liveCaptionSection.shadowRoot!.querySelector<HTMLElement>(
            '#addLanguage');
    const whenDialogOpen = eventToPromise('cr-dialog-open', liveCaptionSection);
    assertTrue(!!addLanguagesButton);
    addLanguagesButton.click();

    await whenDialogOpen;

    dialog = liveCaptionSection.shadowRoot!.querySelector(
        'settings-add-languages-dialog')!;
    assertTrue(!!dialog);
    assertEquals(dialog.id, 'addLanguagesDialog');

    const languageListDiv =
        liveCaptionSection.shadowRoot!.querySelector<HTMLElement>(
            '#languageList');
    assertTrue(!!languageListDiv);

    let languagePacks =
        languageListDiv.querySelectorAll<HTMLElement>('.list-item');
    assertEquals(1, languagePacks.length);

    assertTrue(!!dialog);
    const whenDialogClosed = eventToPromise('close', dialog);
    dialog.dispatchEvent(
        new CustomEvent('languages-added', {detail: ['fr-FR']}));
    dialog.$.dialog.close();
    flush();

    await Promise.all([
      whenDialogClosed,
      browserProxy.whenCalled('installLanguagePacks'),
    ]);

    languagePacks = languageListDiv.querySelectorAll<HTMLElement>('.list-item');
    assertEquals(2, languagePacks.length);

    // Verify that English is marked as the default language.
    assertTrue(languagePacks[0]!.innerText.includes('(default)'));
    assertFalse(languagePacks[1]!.innerText.includes('(default)'));

    // Open the action menu for the French language pack.
    const menuButtons = languageListDiv.querySelectorAll<HTMLElement>(
        'cr-icon-button.icon-more-vert');
    assertEquals(2, menuButtons.length);
    menuButtons[1]!.click();
    const actionMenu =
        liveCaptionSection.shadowRoot!.querySelector('cr-action-menu')!;
    assertTrue(actionMenu.open);

    // Change the default language to French.
    liveCaptionSection.shadowRoot!
        .querySelector<HTMLElement>('#make-default-button')!.click();
    assertFalse(actionMenu.open);
    assertStringExcludes(languagePacks[0]!.innerText, '(default)');
    assertStringContains(languagePacks[1]!.innerText, '(default)');

    // Remove the French language pack and verify that English is the new
    // default language.
    menuButtons[1]!.click();
    liveCaptionSection.shadowRoot!.querySelector<HTMLElement>(
                                      '#remove-button')!.click();
    assertFalse(actionMenu.open);
    assertStringContains(languagePacks[0]!.innerText, '(default)');
    flush();
    languagePacks = languageListDiv.querySelectorAll<HTMLElement>('.list-item');
    assertEquals(1, languagePacks.length);
  });
});
