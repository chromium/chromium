// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

import {CaptionsBrowserProxyImpl, getLanguageHelperInstance, LanguageHelperImpl} from 'chrome://settings/lazy_load.js';
import type {SettingsAddLanguagesDialogElement, SettingsLiveCaptionElement} from 'chrome://settings/lazy_load.js';
import type {SettingsPrefsElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs, loadTimeData, PrefsBrowserProxy, PrefService} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertStringContains, assertStringExcludes, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {getFakeLanguagePrefs} from './fake_language_settings_private.js';
import {TestCaptionsBrowserProxy} from './test_captions_browser_proxy.js';
import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';

function getInitialPrefs(): chrome.settingsPrivate.PrefObject[] {
  return [
    ...getFakeLanguagePrefs(),
    {
      key: 'accessibility.captions.live_caption_enabled',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: false,
    },
    {
      key: 'accessibility.captions.live_caption_mask_offensive_words',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: false,
    },
    {
      key: 'accessibility.captions.live_caption_language',
      type: chrome.settingsPrivate.PrefType.STRING,
      value: 'en-US',
    },
    {
      key: 'accessibility.captions.live_translate_enabled',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: false,
    },
    {
      key: 'accessibility.captions.live_translate_target_language',
      type: chrome.settingsPrivate.PrefType.STRING,
      value: 'en',
    },
  ];
}

suite('LiveCaptionSection', function() {
  let liveCaptionSection: SettingsLiveCaptionElement;
  let settingsPrefs: SettingsPrefsElement;
  let browserProxy: TestCaptionsBrowserProxy;
  let dialog: SettingsAddLanguagesDialogElement|null = null;
  let prefService: PrefService;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      enableLiveCaptionMultiLanguage: true,
      enableLiveTranslate: true,
    });
  });

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    const initialPrefs = getInitialPrefs();
    const prefsBrowserProxy = new TestPrefsBrowserProxy(initialPrefs);
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();
    prefService = PrefService.getInstance();
    await prefService.whenInitialized();

    settingsPrefs = document.createElement('settings-prefs');
    settingsPrefs.initialize(prefsBrowserProxy.fakeApi);
    document.body.appendChild(settingsPrefs);
    await CrSettingsPrefs.initialized;

    LanguageHelperImpl.resetInstanceForTesting();
    const languageHelper = getLanguageHelperInstance();
    await languageHelper.whenReady();

    // Set up test browser proxy.
    browserProxy = new TestCaptionsBrowserProxy();
    CaptionsBrowserProxyImpl.setInstance(browserProxy);

    liveCaptionSection = document.createElement('settings-live-caption');
    document.body.appendChild(liveCaptionSection);
  });

  test('caption.enable toggle', function() {
    const settingsToggle =
        liveCaptionSection.shadowRoot.querySelector<HTMLElement>(
            '#liveCaptionToggleButton');
    assertTrue(!!settingsToggle);

    // Clicking on the toggle switches it to true.
    settingsToggle.click();
    let newToggleValue =
        prefService
            .getPref<boolean>('accessibility.captions.live_caption_enabled')
            .value;
    assertTrue(newToggleValue);

    // Clicking on the toggle switches it to false.
    settingsToggle.click();
    newToggleValue =
        prefService
            .getPref<boolean>('accessibility.captions.live_caption_enabled')
            .value;
    assertFalse(newToggleValue);
  });

  test('add languages and confirm', async function() {
    // Need to make the section visible first, for the innerText calls below to
    // behave correctly.
    const settingsToggle =
        liveCaptionSection.shadowRoot.querySelector<HTMLElement>(
            '#liveCaptionToggleButton');
    assertTrue(!!settingsToggle);
    settingsToggle.click();
    await microtasksFinished();

    const addLanguagesButton =
        liveCaptionSection.shadowRoot.querySelector<HTMLElement>(
            '#addLanguage');
    assertTrue(!!addLanguagesButton);
    assertTrue(isVisible(addLanguagesButton));

    const whenDialogOpen = eventToPromise('cr-dialog-open', liveCaptionSection);
    addLanguagesButton.click();
    await whenDialogOpen;

    dialog = liveCaptionSection.shadowRoot.querySelector(
        'settings-add-languages-dialog');
    assertTrue(!!dialog);
    assertEquals(dialog.id, 'addLanguagesDialog');

    const languageListDiv =
        liveCaptionSection.shadowRoot.querySelector<HTMLElement>(
            '#languageList');
    assertTrue(!!languageListDiv);

    let languagePacks =
        languageListDiv.querySelectorAll<HTMLElement>('.list-item');
    assertEquals(1, languagePacks.length);

    const whenDialogClosed = eventToPromise('close', dialog);
    dialog.dispatchEvent(
        new CustomEvent('languages-added', {detail: ['fr-FR']}));
    dialog.$.dialog.close();

    await Promise.all([
      whenDialogClosed,
      browserProxy.whenCalled('installLanguagePacks'),
    ]);
    await microtasksFinished();

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
        liveCaptionSection.shadowRoot.querySelector('cr-action-menu');
    assertTrue(!!actionMenu);
    assertTrue(actionMenu.open);

    // Change the default language to French.
    const makeDefaultButton =
        liveCaptionSection.shadowRoot.querySelector<HTMLElement>(
            '#make-default-button');
    assertTrue(!!makeDefaultButton);
    makeDefaultButton.click();
    await microtasksFinished();
    assertFalse(actionMenu.open);
    assertStringExcludes(languagePacks[0]!.innerText, '(default)');
    assertStringContains(languagePacks[1]!.innerText, '(default)');

    // Remove the French language pack and verify that English is the new
    // default language.
    const updatedMenuButtons = languageListDiv.querySelectorAll<HTMLElement>(
        'cr-icon-button.icon-more-vert');
    assertEquals(2, updatedMenuButtons.length);
    updatedMenuButtons[1]!.click();
    const removeButton =
        liveCaptionSection.shadowRoot.querySelector<HTMLElement>(
            '#remove-button');
    assertTrue(!!removeButton);
    removeButton.click();
    await microtasksFinished();
    assertFalse(actionMenu.open);
    assertStringContains(languagePacks[0]!.innerText, '(default)');
    languagePacks = languageListDiv.querySelectorAll<HTMLElement>('.list-item');
    assertEquals(1, languagePacks.length);
  });

  test('more action button aria label', async function() {
    await prefService.setPrefValue(
        'accessibility.captions.live_caption_enabled', true);
    await microtasksFinished();

    const defaultLabel = loadTimeData.getString('defaultLanguageLabel');
    const getMoreButtons = () =>
        liveCaptionSection.shadowRoot.querySelectorAll<HTMLElement>(
            'cr-icon-button.icon-more-vert');
    let moreButtons = getMoreButtons();

    const englishButton = moreButtons[0]!;
    assertStringContains(englishButton.ariaLabel!, 'English');
    assertStringContains(englishButton.ariaLabel!, defaultLabel);

    // Add a new language - French.
    const addLanguagesButton =
        liveCaptionSection.shadowRoot.querySelector<HTMLElement>(
            '#addLanguage');
    const whenDialogOpen = eventToPromise('cr-dialog-open', liveCaptionSection);
    assertTrue(!!addLanguagesButton);
    addLanguagesButton.click();
    await whenDialogOpen;

    dialog = liveCaptionSection.shadowRoot.querySelector(
        'settings-add-languages-dialog');
    assertTrue(!!dialog);
    const whenDialogClosed = eventToPromise('close', dialog);
    dialog.dispatchEvent(
        new CustomEvent('languages-added', {detail: ['fr-FR']}));
    dialog.$.dialog.close();

    await Promise.all([
      whenDialogClosed,
      browserProxy.whenCalled('installLanguagePacks'),
    ]);
    await microtasksFinished();

    // The new language (French) should not have the default label.
    moreButtons = getMoreButtons();
    const frenchButton = moreButtons[1]!;
    assertStringContains(frenchButton.ariaLabel!, 'French');
    assertStringExcludes(frenchButton.ariaLabel!, defaultLabel);

    // Change the default language to French.
    frenchButton.click();
    const makeDefaultButton =
        liveCaptionSection.shadowRoot.querySelector<HTMLElement>(
            '#make-default-button');
    assertTrue(!!makeDefaultButton);
    makeDefaultButton.click();
    await microtasksFinished();
    // The English button should no longer have the default label.
    moreButtons = getMoreButtons();
    assertStringContains(moreButtons[0]!.ariaLabel!, 'English');
    assertStringExcludes(moreButtons[0]!.ariaLabel!, defaultLabel);
    // The French button should have the default label.
    assertStringContains(moreButtons[1]!.ariaLabel!, 'French');
    assertStringContains(moreButtons[1]!.ariaLabel!, defaultLabel);
  });
});
