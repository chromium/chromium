// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {CaptionsBrowserProxyImpl, getLanguageHelperInstance, LanguageHelperImpl} from 'chrome://settings/lazy_load.js';
import type {SettingsAddLanguagesDialogElement, SettingsLiveCaptionElement} from 'chrome://settings/lazy_load.js';
import {loadTimeData, PrefsBrowserProxy, PrefService} from 'chrome://settings/settings.js';
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
  let browserProxy: TestCaptionsBrowserProxy;
  let dialog: SettingsAddLanguagesDialogElement|null = null;
  let prefService: PrefService;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      enableLiveCaptionMultiLanguage: true,
      enableLiveTranslate: true,
      enableLiveCaptionSpeechRecognitionSmallExpertModel: false,
      speechRecognitionSmallExpertModelTitle: 'Small expert model',
      speechRecognitionSmallExpertModelSubtitle: 'On-device model',
      speechRecognitionSmallExpertModelDeleteButton: 'Delete model',
      speechRecognitionSmallExpertModelDownloadProgress:
          'Downloading speech recognition files... $1%',
      speechRecognitionSmallExpertModelDownloadComplete:
          'Speech files downloaded',
      speechRecognitionSmallExpertModelDownloadError:
          'Can\'t download speech files. Try again later.',
    });
  });

  teardown(function() {
    loadTimeData.overrideValues({
      enableLiveCaptionSpeechRecognitionSmallExpertModel: false,
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

  test(
      'SpeechRecognitionSmallExpertModel enabled UI elements and hidden ' +
          'language pack list',
      async function() {
        loadTimeData.overrideValues({
          enableLiveCaptionSpeechRecognitionSmallExpertModel: true,
          speechRecognitionSmallExpertModelSubtitle: 'On-device model',
        });
        liveCaptionSection.remove();
        liveCaptionSection = document.createElement('settings-live-caption');
        document.body.appendChild(liveCaptionSection);
        await liveCaptionSection.updateComplete;
        await microtasksFinished();

        await prefService.setPrefValue(
            'accessibility.captions.live_caption_enabled', true);
        await liveCaptionSection.updateComplete;
        await microtasksFinished();

        const speechRecognitionSmallExpertModelSection =
            liveCaptionSection.shadowRoot.querySelector<HTMLElement>(
                '#speechRecognitionSmallExpertModelSection');
        assertTrue(!!speechRecognitionSmallExpertModelSection);
        assertTrue(speechRecognitionSmallExpertModelSection.classList.contains(
            'sub-item'));

        const speechRecognitionSmallExpertModelTitle =
            liveCaptionSection.shadowRoot.querySelector<HTMLElement>(
                '#speechRecognitionSmallExpertModelTitle');
        assertTrue(!!speechRecognitionSmallExpertModelTitle);
        assertEquals(
            'Small expert model',
            speechRecognitionSmallExpertModelTitle.textContent?.trim());

        const secondary =
            speechRecognitionSmallExpertModelSection.querySelector<HTMLElement>(
                '.secondary');
        assertTrue(!!secondary);
        assertEquals('On-device model', secondary.textContent?.trim());

        // Verify title includes size when available.
        webUIListenerCallback(
            'speech-recognition-small-expert-model-state-changed', 'kInstalled',
            100, '', '', '150 MB');
        await liveCaptionSection.updateComplete;
        await microtasksFinished();
        assertEquals(
            'Small expert model (150 MB)',
            speechRecognitionSmallExpertModelTitle.textContent?.trim());
        assertEquals('On-device model', secondary.textContent?.trim());

        const addLanguage =
            liveCaptionSection.shadowRoot.querySelector('#addLanguage');
        assertFalse(!!addLanguage);

        const languageList =
            liveCaptionSection.shadowRoot.querySelector('#languageList');
        assertFalse(!!languageList);
      });

  test(
      'SpeechRecognitionSmallExpertModel downloading and error states',
      async function() {
        loadTimeData.overrideValues({
          enableLiveCaptionSpeechRecognitionSmallExpertModel: true,
          speechRecognitionSmallExpertModelSubtitle: 'On-device model',
        });
        liveCaptionSection.remove();
        liveCaptionSection = document.createElement('settings-live-caption');
        document.body.appendChild(liveCaptionSection);
        await liveCaptionSection.updateComplete;
        await microtasksFinished();

        await prefService.setPrefValue(
            'accessibility.captions.live_caption_enabled', true);
        await liveCaptionSection.updateComplete;
        await microtasksFinished();

        const section =
            liveCaptionSection.shadowRoot.querySelector<HTMLElement>(
                '#speechRecognitionSmallExpertModelSection');
        assertTrue(!!section);

        // Downloading state
        webUIListenerCallback(
            'speech-recognition-small-expert-model-state-changed',
            'kDownloading', 50, '1 MB/s', '10s', '');
        await liveCaptionSection.updateComplete;
        await microtasksFinished();

        let progressSpan =
            section.querySelector<HTMLElement>('span.cr-secondary-text');
        assertTrue(!!progressSpan);
        assertEquals(
            'Downloading speech recognition files... 50% (1 MB/s - 10s)',
            progressSpan.textContent?.trim());

        // Installing state
        webUIListenerCallback(
            'speech-recognition-small-expert-model-state-changed',
            'kInstalling', 100, '', '', '');
        await liveCaptionSection.updateComplete;
        await microtasksFinished();

        progressSpan =
            section.querySelector<HTMLElement>('span.cr-secondary-text');
        assertTrue(!!progressSpan);
        assertEquals(
            'Downloading speech recognition files... 100%',
            progressSpan.textContent?.trim());

        // Error state
        webUIListenerCallback(
            'speech-recognition-small-expert-model-state-changed', 'kError', 0,
            '', '', '');
        await liveCaptionSection.updateComplete;
        await microtasksFinished();

        let errorSpan =
            section.querySelector<HTMLElement>('span.cr-secondary-text');
        assertTrue(!!errorSpan);
        assertEquals(
            'Can\'t download speech files. Try again later.',
            errorSpan.textContent?.trim());

        // Error corrupt persistent state
        webUIListenerCallback(
            'speech-recognition-small-expert-model-state-changed',
            'kErrorCorruptPersistent', 0, '', '', '');
        await liveCaptionSection.updateComplete;
        await microtasksFinished();

        errorSpan =
            section.querySelector<HTMLElement>('span.cr-secondary-text');
        assertTrue(!!errorSpan);
        assertEquals(
            'Can\'t download speech files. Try again later.',
            errorSpan.textContent?.trim());

        // Delete button should not be present in downloading / error states
        let deleteBtn = section.querySelector<HTMLElement>(
            '#deleteSpeechRecognitionSmallExpertModelButton');
        assertFalse(!!deleteBtn);

        // Installed state
        webUIListenerCallback(
            'speech-recognition-small-expert-model-state-changed', 'kInstalled',
            100, '', '', '');
        await liveCaptionSection.updateComplete;
        await microtasksFinished();

        const completeSpan =
            section.querySelector<HTMLElement>('span.cr-secondary-text');
        assertTrue(!!completeSpan);
        assertEquals(
            'Speech files downloaded', completeSpan.textContent?.trim());

        // Delete button should now be present
        deleteBtn = section.querySelector<HTMLElement>(
            '#deleteSpeechRecognitionSmallExpertModelButton');
        assertTrue(!!deleteBtn);

        // Transition back to downloading
        webUIListenerCallback(
            'speech-recognition-small-expert-model-state-changed',
            'kDownloading', 25, '', '', '');
        await liveCaptionSection.updateComplete;
        await microtasksFinished();

        deleteBtn = section.querySelector<HTMLElement>(
            '#deleteSpeechRecognitionSmallExpertModelButton');
        assertFalse(!!deleteBtn);
      });

  test(
      'SpeechRecognitionSmallExpertModel delete button deletes model and ' +
          'sets pref to false',
      async function() {
        loadTimeData.overrideValues({
          enableLiveCaptionSpeechRecognitionSmallExpertModel: true,
          speechRecognitionSmallExpertModelSubtitle: 'On-device model',
        });
        liveCaptionSection.remove();
        liveCaptionSection = document.createElement('settings-live-caption');
        document.body.appendChild(liveCaptionSection);
        await liveCaptionSection.updateComplete;
        await microtasksFinished();

        await prefService.setPrefValue(
            'accessibility.captions.live_caption_enabled', true);
        await liveCaptionSection.updateComplete;
        await microtasksFinished();

        const section =
            liveCaptionSection.shadowRoot.querySelector<HTMLElement>(
                '#speechRecognitionSmallExpertModelSection');
        assertTrue(!!section);

        webUIListenerCallback(
            'speech-recognition-small-expert-model-state-changed', 'kInstalled',
            100, '', '', '');
        await liveCaptionSection.updateComplete;
        await microtasksFinished();

        const deleteBtn = section.querySelector<HTMLElement>(
            '#deleteSpeechRecognitionSmallExpertModelButton');
        assertTrue(!!deleteBtn);

        deleteBtn.click();
        await liveCaptionSection.updateComplete;
        await microtasksFinished();

        const isEnabled =
            prefService
                .getPref<boolean>('accessibility.captions.live_caption_enabled')
                .value;
        assertFalse(isEnabled);

        await browserProxy.whenCalled(
            'deleteSpeechRecognitionSmallExpertModel');
      });

  test(
      'enabling Live Caption with SpeechRecognitionSmallExpertModel ' +
          'invokes installSpeechRecognitionSmallExpertModel',
      async function() {
        loadTimeData.overrideValues({
          enableLiveCaptionSpeechRecognitionSmallExpertModel: true,
        });
        liveCaptionSection.remove();
        liveCaptionSection = document.createElement('settings-live-caption');
        document.body.appendChild(liveCaptionSection);
        await liveCaptionSection.updateComplete;
        await microtasksFinished();

        const toggle = liveCaptionSection.shadowRoot.querySelector<HTMLElement>(
            '#liveCaptionToggleButton');
        assertTrue(!!toggle);
        toggle.click();
        await liveCaptionSection.updateComplete;
        await microtasksFinished();

        await browserProxy.whenCalled(
            'installSpeechRecognitionSmallExpertModel');
      });

  test(
      'SpeechRecognitionSmallExpertModel preserves size on redundant state ' +
          'updates',
      async function() {
        loadTimeData.overrideValues({
          enableLiveCaptionSpeechRecognitionSmallExpertModel: true,
        });
        liveCaptionSection.remove();
        liveCaptionSection = document.createElement('settings-live-caption');
        document.body.appendChild(liveCaptionSection);
        await liveCaptionSection.updateComplete;
        await microtasksFinished();

        await prefService.setPrefValue(
            'accessibility.captions.live_caption_enabled', true);
        await liveCaptionSection.updateComplete;
        await microtasksFinished();

        const section =
            liveCaptionSection.shadowRoot.querySelector<HTMLElement>(
                '#speechRecognitionSmallExpertModelSection');
        assertTrue(!!section);

        const title = liveCaptionSection.shadowRoot.querySelector<HTMLElement>(
            '#speechRecognitionSmallExpertModelTitle');
        assertTrue(!!title);

        // State update with size provided
        webUIListenerCallback(
            'speech-recognition-small-expert-model-state-changed', 'kInstalled',
            100, '', '', '150 MB');
        await liveCaptionSection.updateComplete;
        await microtasksFinished();
        assertEquals('Small expert model (150 MB)', title.textContent?.trim());

        // Subsequent update without size should preserve existing size
        webUIListenerCallback(
            'speech-recognition-small-expert-model-state-changed', 'kInstalled',
            100, '', '', '');
        await liveCaptionSection.updateComplete;
        await microtasksFinished();
        assertEquals('Small expert model (150 MB)', title.textContent?.trim());
      });

  test(
      'reconnecting SettingsLiveCaptionElement restores WebUI listeners',
      async function() {
        loadTimeData.overrideValues({
          enableLiveCaptionSpeechRecognitionSmallExpertModel: true,
        });
        liveCaptionSection.remove();
        liveCaptionSection = document.createElement('settings-live-caption');
        document.body.appendChild(liveCaptionSection);
        await liveCaptionSection.updateComplete;
        await microtasksFinished();

        await prefService.setPrefValue(
            'accessibility.captions.live_caption_enabled', true);
        await liveCaptionSection.updateComplete;
        await microtasksFinished();

        // Simulate navigating away in SPA (disconnecting from DOM)
        liveCaptionSection.remove();
        await microtasksFinished();

        // Simulate navigating back to page (reconnecting to DOM)
        document.body.appendChild(liveCaptionSection);
        await liveCaptionSection.updateComplete;
        await microtasksFinished();

        const section =
            liveCaptionSection.shadowRoot.querySelector<HTMLElement>(
                '#speechRecognitionSmallExpertModelSection');
        assertTrue(!!section);

        // Verify that WebUI listeners are active after reconnection
        webUIListenerCallback(
            'speech-recognition-small-expert-model-state-changed',
            'kDownloading', 75, '500 KB/s', '3s', '');
        await liveCaptionSection.updateComplete;
        await microtasksFinished();

        const progressSpan =
            section.querySelector<HTMLElement>('span.cr-secondary-text');
        assertTrue(!!progressSpan);
        assertEquals(
            'Downloading speech recognition files... 75% (500 KB/s - 3s)',
            progressSpan.textContent?.trim());
      });
});
