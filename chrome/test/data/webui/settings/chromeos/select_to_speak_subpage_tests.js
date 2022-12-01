// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/chromeos/lazy_load.js';

import {CrSettingsPrefs, Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

suite('SelectToSpeakSubpageTests', function() {
  /** @type {SettingsSelectToSpeakSubpageElement} */
  let page = null;

  setup(async function() {
    loadTimeData.overrideValues(
        {isExperimentalAccessibilitySelectToSpeakVoiceSwitchingEnabled: true});

    PolymerTest.clearBody();

    const prefElement = document.createElement('settings-prefs');
    document.body.appendChild(prefElement);

    await CrSettingsPrefs.initialized;
    page = document.createElement('settings-select-to-speak-subpage');
    page.prefs = prefElement.prefs;
    document.body.appendChild(page);
  });

  teardown(function() {
    page.remove();
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
