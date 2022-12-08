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

suite('SelectToSpeakSubpageTests', function() {
  /** @type {SettingsSelectToSpeakSubpageElement} */
  let page = null;

  setup(async function() {
    SelectToSpeakSubpageBrowserProxyImpl.setInstanceForTesting(
        new TestSelectToSpeakSubpageBrowserProxy());

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
