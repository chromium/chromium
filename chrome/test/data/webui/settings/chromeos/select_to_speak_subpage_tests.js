// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/chromeos/lazy_load.js';

import {CrSettingsPrefs, Router, routes, SelectToSpeakSubpageBrowserProxyImpl} from 'chrome://os-settings/chromeos/os_settings.js';
import {addWebUiListener, webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {TestSelectToSpeakSubpageBrowserProxy} from './test_select_to_speak_subpage_browser_proxy.js';

/**
 * Extension ID of the enhanced network TTS voices extension.
 */
const ENHANCED_TTS_EXTENSION_ID = 'jacnkoglebceckolkoapelihnglgaicd';

suite('SelectToSpeakSubpageTests', function() {
  /** @type {SettingsSelectToSpeakSubpageElement} */
  let page = null;
  let browserProxy = null;

  setup(async function() {
    browserProxy = new TestSelectToSpeakSubpageBrowserProxy();
    SelectToSpeakSubpageBrowserProxyImpl.setInstanceForTesting(browserProxy);

    loadTimeData.overrideValues(
        {isExperimentalAccessibilitySelectToSpeakVoiceSwitchingEnabled: true});

    PolymerTest.clearBody();

    const prefElement = document.createElement('settings-prefs');
    document.body.appendChild(prefElement);

    await CrSettingsPrefs.initialized;
    page = document.createElement('settings-select-to-speak-subpage');
    page.prefs = prefElement.prefs;
    document.body.appendChild(page);
    flush();
  });

  teardown(function() {
    page.remove();
  });

  // TODO(crbug.com/1354821): Add tests that the language filter works for
  // enhanced and device voices.

  test('voice pref and dropdown synced', async function() {
    // Make sure voice dropdown is system voice, matching default pref state.
    const voiceDropdown = page.shadowRoot.querySelector('#voiceDropdown');
    await waitAfterNextRender(voiceDropdown);
    const voiceSelectElement = voiceDropdown.shadowRoot.querySelector('select');
    assertEquals('select_to_speak_system_voice', voiceSelectElement.value);

    // Change voice to Chrome OS US English, and verify pref is also changed.
    voiceSelectElement.value = 'Chrome OS US English';
    voiceSelectElement.dispatchEvent(new CustomEvent('change'));
    flush();
    const voicePref = page.getPref('settings.a11y.select_to_speak_voice_name');
    assertEquals('Chrome OS US English', voicePref.value);
  });

  test('voice preview text field and button sends sample message', async () => {
    // Make sure preview input exists, and write a sample message into it.
    const voicePreviewInput =
        page.shadowRoot.querySelector('#voicePreviewInput');
    assertTrue(!!voicePreviewInput);
    voicePreviewInput.value = 'The quick brown fox jumped over the lazy dog.';

    // Click preview button, expect sample message to be sent.
    page.shadowRoot.querySelector('#voicePreviewButton').click();
    const [previewText, previewVoice] =
        await browserProxy.whenCalled('previewTtsVoice');
    assertEquals(previewText, 'The quick brown fox jumped over the lazy dog.');
    assertEquals(previewVoice, '{"name":"","extension":""}');
  });

  test('voice switching pref and toggle synced', function() {
    // Make sure voice switching toggle is off, matching default pref state.
    const voiceSwitchingToggle =
        page.shadowRoot.querySelector('#voiceSwitchingToggle');
    assertFalse(voiceSwitchingToggle.checked);

    // Toggle voice switching on, and verify voice_switching pref is enabled.
    voiceSwitchingToggle.click();
    const voiceSwitchingPref =
        page.getPref('settings.a11y.select_to_speak_voice_switching');
    assertTrue(voiceSwitchingPref.value);
  });

  test('enhanced network voices pref and toggle synced', function() {
    // Make sure enhanced network voices toggle is off, matching default pref
    // state.
    const enhancedNetworkVoicesToggle =
        page.shadowRoot.querySelector('#enhancedNetworkVoicesToggle');
    assertFalse(enhancedNetworkVoicesToggle.checked);

    // Toggle enhanced network voices on, and verify voice_switching pref is
    // enabled.
    enhancedNetworkVoicesToggle.click();
    const enhancedNetworkVoicesPref =
        page.getPref('settings.a11y.select_to_speak_enhanced_network_voices');
    assertTrue(enhancedNetworkVoicesPref.value);
  });

  test('enhanced network voices toggle respects enterprise policy', function() {
    // Make sure enhanced network voices toggle is togglable and off, matching
    // default pref state, and verify enterprise managed icon + controls are not
    // present.
    const enhancedNetworkVoicesToggle =
        page.shadowRoot.querySelector('#enhancedNetworkVoicesToggle');
    assertFalse(
        enhancedNetworkVoicesToggle.controlDisabled(),
        'enhanced voices toggle should be togglable');
    assertFalse(
        enhancedNetworkVoicesToggle.checked,
        'enhanced voices toggle should be off');
    const getManagedIcon = () =>
        enhancedNetworkVoicesToggle.shadowRoot.querySelector(
            'cr-policy-pref-indicator');
    const managedIconVisible = () => getManagedIcon().style.display !== 'none';
    const getEnhancedVoiceControls = () =>
        page.shadowRoot.querySelector('#enhancedNetworkVoiceControls');
    const enhancedVoiceControlsVisible = () =>
        getEnhancedVoiceControls().style.display !== 'none';
    assertEquals(null, getManagedIcon(), 'managed icon should not be present');
    assertEquals(
        null, getEnhancedVoiceControls(),
        'enhanced voice controls should not be present');

    // Toggle enhanced network voices on, and verify voice_switching pref is
    // enabled, toggle is on, enterprise managed icon is not present, and
    // controls are visible.
    enhancedNetworkVoicesToggle.click();
    flush();
    const enhancedNetworkVoicesPref =
        page.getPref('settings.a11y.select_to_speak_enhanced_network_voices');
    assertTrue(
        enhancedNetworkVoicesPref.value,
        'enhanced voices pref should be enabled');
    assertTrue(
        enhancedNetworkVoicesToggle.checked,
        'enhanced voices toggle should be on');
    assertEquals(
        null, getManagedIcon(), 'managed icon should still not be present');
    assertTrue(
        enhancedVoiceControlsVisible(),
        'enhanced voice controls should be visible');

    // Disallow enhanced voices via enterprise policy.
    page.setPrefValue(
        'settings.a11y.enhanced_network_voices_in_select_to_speak_allowed',
        false);
    flush();

    // Verify voice switching toggle is immediately disabled and off, enterprise
    // managed icon is visible, and controls are not visible.
    assertTrue(
        enhancedNetworkVoicesToggle.controlDisabled(),
        'enhanced voices toggle should not be togglable');
    assertFalse(
        enhancedNetworkVoicesToggle.checked,
        'enhanced voices toggle should be off again');
    assertTrue(managedIconVisible(), 'managed icon should be visible');
    assertFalse(
        enhancedVoiceControlsVisible(),
        'enhanced voice controls should not be visible');

    // Assert pref is still enabled (we don't disable the user's pref just
    // because the enterprise policy pref is disabled).
    assertTrue(
        enhancedNetworkVoicesPref.value,
        'enhanced voices pref should still be enabled');

    // Reallow enhanced voices via enterprise policy.
    page.setPrefValue(
        'settings.a11y.enhanced_network_voices_in_select_to_speak_allowed',
        true);
    flush();

    // Verify voice switching toggle is togglable and turned back on again,
    // enterprise managed icon is not visible, and controls are visible.
    assertFalse(
        enhancedNetworkVoicesToggle.controlDisabled(),
        'enhanced voices toggle should be togglable again');
    assertTrue(
        enhancedNetworkVoicesToggle.checked,
        'enhanced voices toggle should be on again');
    assertFalse(managedIconVisible(), 'managed icon should not be visible');
    assertTrue(
        enhancedVoiceControlsVisible(),
        'enhanced voice controls should be visible again');
  });

  test('enhanced network voice pref and dropdown synced', async function() {
    // Turn on enhanced network voices.
    const enhancedNetworkVoicesToggle =
        page.shadowRoot.querySelector('#enhancedNetworkVoicesToggle');
    enhancedNetworkVoicesToggle.click();
    flush();

    // Make sure enhanced network voice dropdown is default voice, matching
    // default pref state.
    const enhancedNetworkVoiceDropdown =
        page.shadowRoot.querySelector('#enhancedNetworkVoiceDropdown');
    await waitAfterNextRender(enhancedNetworkVoiceDropdown);
    const enhancedNetworkVoiceSelectElement =
        enhancedNetworkVoiceDropdown.shadowRoot.querySelector('select');
    assertEquals('default-wavenet', enhancedNetworkVoiceSelectElement.value);

    // Change voice to Bangla (India) 1, and verify pref is also changed.
    enhancedNetworkVoiceSelectElement.value = 'bnm';
    enhancedNetworkVoiceSelectElement.dispatchEvent(new CustomEvent('change'));
    const enhancedNetworkVoicePref =
        page.getPref('settings.a11y.select_to_speak_enhanced_voice_name');
    assertEquals('bnm', enhancedNetworkVoicePref.value);
  });

  test('enhanced network voices not in primary voice dropdown', async () => {
    // Turn on enhanced network voices.
    const enhancedNetworkVoicesToggle =
        page.shadowRoot.querySelector('#enhancedNetworkVoicesToggle');
    enhancedNetworkVoicesToggle.click();
    flush();

    // Get all of the voices from the primary voice dropdown.
    const voiceDropdown = page.shadowRoot.querySelector('#voiceDropdown');
    await waitAfterNextRender(voiceDropdown);
    const voiceSelectElement = voiceDropdown.shadowRoot.querySelector('select');
    const voices = [...voiceSelectElement.options].map(({value}) => value);

    // Make sure none of the voices are enhanced network voices.
    page.voices_
        .filter(
            pageVoice => voices.find(voice => voice === pageVoice.voiceName))
        .forEach(
            ({extensionId}) =>
                assertNotEquals(extensionId, ENHANCED_TTS_EXTENSION_ID));
  });

  test('enhanced network voice preview sends sample message', async () => {
    // Turn on enhanced network voices.
    const enhancedNetworkVoicesToggle =
        page.shadowRoot.querySelector('#enhancedNetworkVoicesToggle');
    enhancedNetworkVoicesToggle.click();
    flush();

    // Make sure enhanced network preview input exists, and write a sample
    // message into it.
    const enhancedNetworkVoicePreviewInput =
        page.shadowRoot.querySelector('#enhancedNetworkVoicePreviewInput');
    assertTrue(!!enhancedNetworkVoicePreviewInput);
    enhancedNetworkVoicePreviewInput.value =
        'The quick brown fox jumped over the lazy dog.';

    // Click preview button, expect sample message to be sent.
    page.shadowRoot.querySelector('#enhancedNetworkVoicePreviewButton').click();
    const [previewText, previewVoice] =
        await browserProxy.whenCalled('previewTtsVoice');
    assertEquals(previewText, 'The quick brown fox jumped over the lazy dog.');
    assertEquals(
        previewVoice,
        '{"name":"default-wavenet","extension":"jacnkoglebceckolkoapelihnglgaicd"}');
  });

  test(
      'voice preview buttons and inputs enabled when not speaking and disabled when speaking',
      async () => {
        // Turn on enhanced network voices.
        const enhancedNetworkVoicesToggle =
            page.shadowRoot.querySelector('#enhancedNetworkVoicesToggle');
        enhancedNetworkVoicesToggle.click();
        flush();

        // Get all voice preview buttons and inputs.
        const voicePreviewElements = [
          page.shadowRoot.querySelector('#voicePreviewButton'),
          page.shadowRoot.querySelector('#voicePreviewInput'),
          page.shadowRoot.querySelector('#enhancedNetworkVoicePreviewButton'),
          page.shadowRoot.querySelector('#enhancedNetworkVoicePreviewInput'),
        ];

        // Make sure voice preview buttons and inputs are not disabled.
        voicePreviewElements.forEach(button => assertFalse(button.disabled));

        // Simulate TTS voice speaking.
        webUIListenerCallback('tts-preview-state-changed', true);

        // Make sure voice preview buttons and inputs are disabled.
        voicePreviewElements.forEach(button => assertTrue(button.disabled));
      });

  test(
      'voice preview buttons and inputs enabled when not empty and disabled when empty',
      async () => {
        // Turn on enhanced network voices.
        const enhancedNetworkVoicesToggle =
            page.shadowRoot.querySelector('#enhancedNetworkVoicesToggle');
        enhancedNetworkVoicesToggle.click();
        flush();

        // Get voice preview buttons and inputs.
        const voicePreviewButton =
            page.shadowRoot.querySelector('#voicePreviewButton');
        const voicePreviewInput =
            page.shadowRoot.querySelector('#voicePreviewInput');
        const enhancedNetworkVoicePreviewButton =
            page.shadowRoot.querySelector('#enhancedNetworkVoicePreviewButton');
        const enhancedNetworkVoicePreviewInput =
            page.shadowRoot.querySelector('#enhancedNetworkVoicePreviewInput');

        // Make sure voice preview buttons and inputs are not disabled.
        assertFalse(voicePreviewButton.disabled);
        assertFalse(voicePreviewInput.disabled);
        assertFalse(enhancedNetworkVoicePreviewButton.disabled);
        assertFalse(enhancedNetworkVoicePreviewInput.disabled);

        // Clear primary voice preview input. Make sure only primary voice
        // preview button is disabled.
        voicePreviewInput.value = '';
        assertTrue(voicePreviewButton.disabled);
        assertFalse(voicePreviewInput.disabled);
        assertFalse(enhancedNetworkVoicePreviewButton.disabled);
        assertFalse(enhancedNetworkVoicePreviewInput.disabled);

        // Clear enhanced network voice preview input. Make sure both voice
        // preview buttons are disabled.
        enhancedNetworkVoicePreviewInput.value = '';
        assertTrue(voicePreviewButton.disabled);
        assertFalse(voicePreviewInput.disabled);
        assertTrue(enhancedNetworkVoicePreviewButton.disabled);
        assertFalse(enhancedNetworkVoicePreviewInput.disabled);

        // Add text back to the primary voice preview input. Make sure only
        // enhanced network voice preview button is disabled.
        voicePreviewInput.value = 'Testing';
        assertFalse(voicePreviewButton.disabled);
        assertFalse(voicePreviewInput.disabled);
        assertTrue(enhancedNetworkVoicePreviewButton.disabled);
        assertFalse(enhancedNetworkVoicePreviewInput.disabled);

        // Add text back to the enhanced network voice preview input. Make sure
        // all elements are enabled.
        enhancedNetworkVoicePreviewInput.value =
            'Enhanced Network Voice Testing';
        assertFalse(voicePreviewButton.disabled);
        assertFalse(voicePreviewInput.disabled);
        assertFalse(enhancedNetworkVoicePreviewButton.disabled);
        assertFalse(enhancedNetworkVoicePreviewInput.disabled);
      });

  test('word highlight pref and toggle synced', function() {
    // Make sure word highlight toggle is on, matching default pref state.
    const wordHighlightToggle =
        page.shadowRoot.querySelector('#wordHighlightToggle');
    assertTrue(wordHighlightToggle.checked);

    // Toggle word highlighting off, and verify word_highlight pref is enabled.
    wordHighlightToggle.click();
    const wordHighlightPref =
        page.getPref('settings.a11y.select_to_speak_word_highlight');
    assertFalse(wordHighlightPref.value);
  });

  test('background shading pref and toggle synced', function() {
    // Make sure background shading toggle is off, matching default pref state.
    const backgroundShadingToggle =
        page.shadowRoot.querySelector('#backgroundShadingToggle');
    assertFalse(backgroundShadingToggle.checked);

    // Toggle background shading on, and verify pref is enabled.
    backgroundShadingToggle.click();
    const backgroundShadingPref =
        page.getPref('settings.a11y.select_to_speak_background_shading');
    assertTrue(backgroundShadingPref.value);
  });

  test('navigation controls pref and toggle synced', function() {
    // Make sure navigation controls toggle is on, matching default pref state.
    const navigationControlsToggle =
        page.shadowRoot.querySelector('#navigationControlsToggle');
    assertTrue(navigationControlsToggle.checked);

    // Toggle navigation controls off, and verify pref is enabled.
    navigationControlsToggle.click();
    const navigationControlsPref =
        page.getPref('settings.a11y.select_to_speak_navigation_controls');
    assertFalse(navigationControlsPref.value);
  });

  test('highlight color pref and dropdown synced', async function() {
    // Make sure highlight color dropdown is blue, matching default pref state.
    const highlightColorDropdown =
        page.shadowRoot.querySelector('#highlightColorDropdown');
    await waitAfterNextRender(highlightColorDropdown);
    const highlightColorSelectElement =
        highlightColorDropdown.shadowRoot.querySelector('select');
    assertEquals('#5e9bff', highlightColorSelectElement.value);

    // Turn highlight color to orange, and verify pref is also orange.
    highlightColorSelectElement.value = '#ffa13d';
    highlightColorSelectElement.dispatchEvent(new CustomEvent('change'));
    const highlightColorPref =
        page.getPref('settings.a11y.select_to_speak_highlight_color');
    assertEquals('#ffa13d', highlightColorPref.value);
  });
});
