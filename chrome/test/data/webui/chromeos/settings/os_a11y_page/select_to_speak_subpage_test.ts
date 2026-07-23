// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import type {SettingsSelectToSpeakSubpageElement} from 'chrome://os-settings/lazy_load.js';
import type {SettingsPrefsElement, SettingsToggleButtonElement} from 'chrome://os-settings/os_settings.js';
import {CrSettingsPrefs, SelectToSpeakSubpageBrowserProxyImpl} from 'chrome://os-settings/os_settings.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {TestSelectToSpeakSubpageBrowserProxy} from './test_select_to_speak_subpage_browser_proxy.js';

suite('<settings-select-to-speak-subpage>', () => {
  let page: SettingsSelectToSpeakSubpageElement;
  let browserProxy: TestSelectToSpeakSubpageBrowserProxy;
  let prefElement: SettingsPrefsElement;

  setup(async () => {
    browserProxy = new TestSelectToSpeakSubpageBrowserProxy();
    SelectToSpeakSubpageBrowserProxyImpl.setInstanceForTesting(browserProxy);

    prefElement = document.createElement('settings-prefs');
    document.body.appendChild(prefElement);

    await CrSettingsPrefs.initialized;
    page = document.createElement('settings-select-to-speak-subpage');
    page.prefs = prefElement.prefs!;
    document.body.appendChild(page);

    // V2 controls use one-way bindings and dispatch events instead of updating
    // prefs via Polymer two-way bindings. We must manually bridge these
    // updates.
    page.addEventListener('user-action-setting-pref-change', (event: Event) => {
      const {prefKey, value} = (event as CustomEvent).detail;
      prefElement.set(`prefs.${prefKey}.value`, value);
    });

    flush();
  });

  teardown(() => {
    page.remove();
    prefElement.remove();
    browserProxy.reset();
  });

  // TODO(crbug.com/1354821): Add tests that the language filter works for
  // enhanced and device voices.

  function waitForPrefChangeEvent(key: string, value: any): Promise<void> {
    return new Promise((resolve) => {
      const listener = (prefs: chrome.settingsPrivate.PrefObject[]) => {
        const pref = prefs.find(p => p.key === key);
        if (pref && pref.value === value) {
          chrome.settingsPrivate.onPrefsChanged.removeListener(listener);
          resolve();
        }
      };
      chrome.settingsPrivate.onPrefsChanged.addListener(listener);
    });
  }

  test('voice pref and dropdown synced', async () => {
    // Make sure voice dropdown is system voice, matching default pref state.
    const voiceDropdown =
        page.shadowRoot!.querySelector<HTMLElement>('#voiceDropdown');
    assertTrue(!!voiceDropdown);
    await waitAfterNextRender(voiceDropdown);
    const voiceSelectElement =
        voiceDropdown.shadowRoot!.querySelector<HTMLElement>('#dropdown')!
            .shadowRoot!.querySelector<HTMLSelectElement>('select');
    assertTrue(!!voiceSelectElement);
    assertEquals('select_to_speak_system_voice', voiceSelectElement.value);

    // Change voice to Chrome OS US English, and verify pref is also changed.
    const changePromise = waitForPrefChangeEvent(
        'settings.a11y.select_to_speak_voice_name', 'Chrome OS US English');
    voiceSelectElement.value = 'Chrome OS US English';
    voiceSelectElement.dispatchEvent(new CustomEvent('change'));
    flush();
    await changePromise;
    const voicePref = page.getPref('settings.a11y.select_to_speak_voice_name');
    assertEquals('Chrome OS US English', voicePref.value);

    // Reset to default to avoid affecting other tests.
    const resetPromise = waitForPrefChangeEvent(
        'settings.a11y.select_to_speak_voice_name',
        'select_to_speak_system_voice');
    voiceSelectElement.value = 'select_to_speak_system_voice';
    voiceSelectElement.dispatchEvent(new CustomEvent('change'));
    flush();
    await resetPromise;
  });

  test('voice preview text field and button sends sample message', async () => {
    // Make sure preview input exists, and write a sample message into it.
    const voicePreviewInput =
        page.shadowRoot!.querySelector<HTMLInputElement>('#voicePreviewInput');
    assertTrue(!!voicePreviewInput);
    voicePreviewInput.value = 'The quick brown fox jumped over the lazy dog.';

    // Click preview button, expect sample message to be sent.
    const voicePreviewButton =
        page.shadowRoot!.querySelector<HTMLButtonElement>(
            '#voicePreviewButton');
    assertTrue(!!voicePreviewButton);
    voicePreviewButton.click();
    const [previewText, previewVoice] =
        await browserProxy.whenCalled('previewTtsVoice');
    assertEquals('The quick brown fox jumped over the lazy dog.', previewText);
    assertEquals('{"name":"","extension":""}', previewVoice);
  });

  test('voice switching pref and toggle synced', () => {
    // Make sure voice switching toggle is off, matching default pref state.
    const voiceSwitchingToggle =
        page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#voiceSwitchingToggle');
    assertTrue(!!voiceSwitchingToggle);
    assertFalse(voiceSwitchingToggle.checked);

    // Toggle voice switching on, and verify voice_switching pref is enabled.
    voiceSwitchingToggle.click();
    const voiceSwitchingPref =
        page.getPref<boolean>('settings.a11y.select_to_speak_voice_switching');
    assertTrue(voiceSwitchingPref.value);
  });

  test(
      'voice preview button and input enabled when not speaking and disabled when speaking',
      () => {
        // Get voice preview button and input.
        const voicePreviewElements:
            Array<HTMLInputElement|HTMLButtonElement|null> = [
              page.shadowRoot!.querySelector('#voicePreviewButton'),
              page.shadowRoot!.querySelector('#voicePreviewInput'),
            ];

        // Make sure voice preview button and input are not disabled.
        voicePreviewElements.forEach(button => {
          assertTrue(!!button);
          assertFalse(button.disabled);
        });

        // Simulate TTS voice speaking.
        webUIListenerCallback('tts-preview-state-changed', true);

        // Make sure voice preview button and input are disabled.
        voicePreviewElements.forEach(button => {
          assertTrue(!!button);
          assertTrue(button.disabled);
        });
      });

  test(
      'voice preview button and input enabled when not empty and disabled when empty',
      () => {
        // Get voice preview button and input.
        const voicePreviewButton =
            page.shadowRoot!.querySelector<HTMLButtonElement>(
                '#voicePreviewButton');
        const voicePreviewInput =
            page.shadowRoot!.querySelector<HTMLInputElement>(
                '#voicePreviewInput');

        assertTrue(!!voicePreviewButton);
        assertTrue(!!voicePreviewInput);

        // Make sure voice preview button and input are not disabled.
        assertFalse(voicePreviewButton.disabled);
        assertFalse(voicePreviewInput.disabled);

        // Clear voice preview input. Make sure the voice preview button is
        // disabled.
        voicePreviewInput.value = '';
        assertTrue(voicePreviewButton.disabled);
        assertFalse(voicePreviewInput.disabled);

        // Add text back to the voice preview input. Make sure all elements are
        // enabled.
        voicePreviewInput.value = 'Testing';
        assertFalse(voicePreviewButton.disabled);
        assertFalse(voicePreviewInput.disabled);
      });

  test('word highlight pref and toggle synced', () => {
    // Make sure word highlight toggle is on, matching default pref state.
    const wordHighlightToggle =
        page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#wordHighlightToggle');
    assertTrue(!!wordHighlightToggle);
    assertTrue(wordHighlightToggle.checked);

    // Toggle word highlighting off, and verify word_highlight pref is enabled.
    wordHighlightToggle.click();
    const wordHighlightPref =
        page.getPref<boolean>('settings.a11y.select_to_speak_word_highlight');
    assertFalse(wordHighlightPref.value);
  });

  test('background shading pref and toggle synced', () => {
    // Make sure background shading toggle is off, matching default pref state.
    const backgroundShadingToggle =
        page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#backgroundShadingToggle');
    assertTrue(!!backgroundShadingToggle);
    assertFalse(backgroundShadingToggle.checked);

    // Toggle background shading on, and verify pref is enabled.
    backgroundShadingToggle.click();
    const backgroundShadingPref = page.getPref<boolean>(
        'settings.a11y.select_to_speak_background_shading');
    assertTrue(backgroundShadingPref.value);
  });

  test('navigation controls pref and toggle synced', () => {
    // Make sure navigation controls toggle is on, matching default pref state.
    const navigationControlsToggle =
        page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#navigationControlsToggle');
    assertTrue(!!navigationControlsToggle);
    assertTrue(navigationControlsToggle.checked);

    // Toggle navigation controls off, and verify pref is enabled.
    navigationControlsToggle.click();
    const navigationControlsPref = page.getPref<boolean>(
        'settings.a11y.select_to_speak_navigation_controls');
    assertFalse(navigationControlsPref.value);
  });

  test('highlight color pref and dropdown synced', async () => {
    // Make sure highlight color dropdown is blue, matching default pref state.
    const highlightColorDropdown =
        page.shadowRoot!.querySelector<HTMLElement>('#highlightColorDropdown');
    assertTrue(!!highlightColorDropdown);
    await waitAfterNextRender(highlightColorDropdown);
    const highlightColorSelectElement =
        highlightColorDropdown.shadowRoot!.querySelector('select');
    assertTrue(!!highlightColorSelectElement);
    assertEquals('#5e9bff', highlightColorSelectElement.value);

    // Turn highlight color to orange, and verify pref is also orange.
    highlightColorSelectElement.value = '#ffa13d';
    highlightColorSelectElement.dispatchEvent(new CustomEvent('change'));
    const highlightColorPref =
        page.getPref('settings.a11y.select_to_speak_highlight_color');
    assertEquals('#ffa13d', highlightColorPref.value);
  });
});
