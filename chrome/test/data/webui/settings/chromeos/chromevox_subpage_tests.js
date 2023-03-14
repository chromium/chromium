// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for the ChromeVox subpage in ChromeOS Settings.
 */

import 'chrome://os-settings/chromeos/lazy_load.js';

import {ChromeVoxSubpageBrowserProxyImpl, CrSettingsPrefs, Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
import {addWebUiListener, webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertDeepEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {TestChromeVoxSubpageBrowserProxy} from './test_chromevox_subpage_browser_proxy.js';

suite('ChromeVoxSubpageTests', function() {
  /** @type {SettingsChromeVoxSubpageElement} */
  let page = null;
  let browserProxy = null;

  setup(async function() {
    browserProxy = new TestChromeVoxSubpageBrowserProxy();
    ChromeVoxSubpageBrowserProxyImpl.setInstanceForTesting(browserProxy);

    PolymerTest.clearBody();

    const prefElement = document.createElement('settings-prefs');
    document.body.appendChild(prefElement);

    await CrSettingsPrefs.initialized;
    page = document.createElement('settings-chromevox-subpage');
    page.prefs = prefElement.prefs;
    document.body.appendChild(page);
    flush();
  });

  teardown(function() {
    page.remove();
  });

  test('voices are ordered', async function() {
    // Make sure voices are ordered with the system default voice first, then
    // Google voices, then eSpeak, then local, then remote.
    const voiceDropdown = page.shadowRoot.querySelector('#voiceDropdown');
    const expectedMenuOptions = [
      {name: 'System Text-to-Speech voice', value: 'chromeos_system_voice'},
      {name: 'Chrome OS US English', value: 'Chrome OS US English'},
      {name: 'Chrome OS हिन्दी', value: 'Chrome OS हिन्दी'},
      {name: 'eSpeak Turkish', value: 'eSpeak Turkish'},
      {name: 'default-coolnet', value: 'default-coolnet'},
      {name: 'bnm', value: 'bnm'},
      {name: 'bnx', value: 'bnx'},
    ];
    assertDeepEquals(expectedMenuOptions, voiceDropdown.menuOptions);
  });

  test('voice pref and dropdown synced', async function() {
    // Make sure voice dropdown is system voice, matching default pref state.
    const voiceDropdown = page.shadowRoot.querySelector('#voiceDropdown');
    await waitAfterNextRender(voiceDropdown);
    const voiceSelectElement = voiceDropdown.shadowRoot.querySelector('select');
    assertEquals('chromeos_system_voice', voiceSelectElement.value);

    // Change voice to Chrome OS US English, and verify pref is also changed.
    voiceSelectElement.value = 'Chrome OS US English';
    voiceSelectElement.dispatchEvent(new CustomEvent('change'));
    flush();
    const voicePref = page.getPref('settings.a11y.chromevox.voice_name');
    assertEquals('Chrome OS US English', voicePref.value);
  });

  test('language switching pref and toggle synced', function() {
    // Make sure language switching toggle is off, matching default pref state.
    const languageSwitchingToggle =
        page.shadowRoot.querySelector('#languageSwitchingToggle');
    assertFalse(languageSwitchingToggle.checked);

    // Toggle language switching on, and verify language_switching pref is
    // enabled.
    languageSwitchingToggle.click();
    const languageSwitchingPref =
        page.getPref('settings.a11y.chromevox.language_switching');
    assertTrue(languageSwitchingPref.value);
  });
});
