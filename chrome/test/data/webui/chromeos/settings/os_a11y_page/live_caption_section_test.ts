// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';
import 'chrome://os-settings/lazy_load.js';

import type {OsSettingsAddItemsDialogElement, SettingsLiveCaptionElement} from 'chrome://os-settings/lazy_load.js';
import {CaptionsBrowserProxyImpl} from 'chrome://os-settings/lazy_load.js';
import {CrSettingsPrefs} from 'chrome://os-settings/os_settings.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNull, assertStringContains, assertStringExcludes, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {fakeDataBind} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {clearBody} from '../utils.js';

import {TestCaptionsBrowserProxy} from './test_captions_browser_proxy.js';

suite('LiveCaptionSection', () => {
  let liveCaptionSection: SettingsLiveCaptionElement;
  let browserProxy: TestCaptionsBrowserProxy;
  let dialog: OsSettingsAddItemsDialogElement|null = null;

  suiteSetup(() => {
    loadTimeData.overrideValues({
      enableLiveCaptionMultiLanguage: true,
      enableLiveTranslate: true,
    });
  });

  setup(() => {
    // Set up test browser proxy.
    browserProxy = new TestCaptionsBrowserProxy();
    CaptionsBrowserProxyImpl.setInstance(browserProxy);
  });

  /** Sets up the element for test. Call this after overriding loadTimeData. */
  async function setupLiveCaptionSection() {
    const settingsPrefs = document.createElement('settings-prefs');
    clearBody();

    const settingsLanguages = document.createElement('settings-languages');
    settingsLanguages.prefs = settingsPrefs.prefs;
    fakeDataBind(settingsPrefs, settingsLanguages, 'prefs');
    document.body.appendChild(settingsLanguages);

    document.body.appendChild(settingsPrefs);
    await CrSettingsPrefs.initialized;

    liveCaptionSection = document.createElement('settings-live-caption');
    liveCaptionSection.prefs = settingsPrefs.prefs;
    fakeDataBind(settingsPrefs, liveCaptionSection, 'prefs');
    liveCaptionSection.languageHelper = settingsLanguages.languageHelper;
    fakeDataBind(settingsLanguages, liveCaptionSection, 'language-helper');
    document.body.appendChild(liveCaptionSection);

    flush();
    await liveCaptionSection.languageHelper.whenReady();
  }

  test('test caption.enable toggle', async () => {
    await setupLiveCaptionSection();
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


  test('add languages and display download progress', async () => {
    await setupLiveCaptionSection();
    const addLanguagesButton =
        liveCaptionSection.shadowRoot!.querySelector<HTMLElement>(
            '#addLanguage');
    const whenDialogOpen = eventToPromise('cr-dialog-open', liveCaptionSection);
    assertTrue(!!addLanguagesButton);
    addLanguagesButton.click();

    await whenDialogOpen;

    dialog = liveCaptionSection.shadowRoot!.querySelector(
        'os-settings-add-items-dialog')!;
    assertTrue(!!dialog);
    assertEquals(dialog.id, 'liveCaptionsAddLanguagesDialog');

    const languageListDiv =
        liveCaptionSection.shadowRoot!.querySelector<HTMLElement>(
            '#languageList');
    assertTrue(!!languageListDiv);

    let languagePacks =
        languageListDiv.querySelectorAll<HTMLElement>('.list-item');
    assertEquals(1, languagePacks.length);

    assertTrue(!!dialog);
    const whenDialogClosed = eventToPromise('close', dialog);
    dialog.dispatchEvent(new CustomEvent('items-added', {detail: ['fr-FR']}));
    dialog.$.dialog.close();
    flush();
    webUIListenerCallback('soda-download-progress-changed', '17', 'fr-FR');
    flush();
    languagePacks = languageListDiv.querySelectorAll<HTMLElement>('.list-item');
    assertEquals(2, languagePacks.length);

    // Verify that English is marked as the default language.
    assertStringContains(languagePacks[0]!.innerText, '(default)');
    assertStringExcludes(languagePacks[1]!.innerText, '(default)');
    let hasProgress = false;
    languagePacks.forEach((lp) => {
      if (lp.innerText.includes('17')) {
        hasProgress = true;
      }
    });
    assertTrue(hasProgress);

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

    await Promise.all([
      whenDialogClosed,
      browserProxy.whenCalled('installLanguagePacks'),
    ]);
  });

  test('shows live translate if feature enabled', async () => {
    loadTimeData.overrideValues({enableLiveTranslate: true});
    await setupLiveCaptionSection();

    const liveTranslateSection =
        liveCaptionSection.shadowRoot!.querySelector('settings-live-translate');
    assertTrue(!!liveTranslateSection);
  });

  test('does not show live translate if feature disabled', async () => {
    loadTimeData.overrideValues({enableLiveTranslate: false});
    await setupLiveCaptionSection();

    const liveTranslateSection =
        liveCaptionSection.shadowRoot!.querySelector('settings-live-translate');
    assertNull(liveTranslateSection);
  });
});
