// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {SettingsOfferWritingHelpPageElement, SettingsPrefsElement} from 'chrome://settings/settings.js';
import {COMPOSE_PROACTIVE_NUDGE_PREF, CrSettingsPrefs} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('OfferWritingHelpPage', function() {
  let page: SettingsOfferWritingHelpPageElement;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  function createPage() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-offer-writing-help-page');
    page.prefs = settingsPrefs.prefs;
    document.body.appendChild(page);
  }

  // Test that interacting with the main toggle updates the corresponding pref.
  test('MainToggle', () => {
    createPage();
    page.setPrefValue(COMPOSE_PROACTIVE_NUDGE_PREF, false);

    const mainToggle = page.shadowRoot!.querySelector('settings-toggle-button');
    assertTrue(!!mainToggle);

    // Check disabled case.
    assertFalse(mainToggle.checked);

    // Check enabled case.
    mainToggle.click();
    assertEquals(true, page.getPref(COMPOSE_PROACTIVE_NUDGE_PREF).value);
    assertTrue(mainToggle.checked);
  });
});
