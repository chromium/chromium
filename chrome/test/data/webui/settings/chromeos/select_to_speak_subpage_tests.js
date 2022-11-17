// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/chromeos/lazy_load.js';

import {Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender, waitBeforeNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

suite('SelectToSpeakSubpageTests', function() {
  let page = null;

  function initPage() {
    page = document.createElement('settings-select-to-speak-subpage');
    document.body.appendChild(page);
  }

  setup(function() {
    PolymerTest.clearBody();
    Router.getInstance().navigateTo(routes.A11Y_SELECT_TO_SPEAK);
  });

  teardown(function() {
    if (page) {
      page.remove();
    }
    Router.getInstance().resetRouteForTesting();
  });

  test('word highlight pref and toggle synced', async function() {
    initPage();

    // Make sure word highlight toggle is off, matching default pref state.
    const wordHighlightToggle =
        page.shadowRoot.querySelector('#a11ySelectToSpeakOptionsHighlight');
    assertFalse(wordHighlightToggle.checked);

    // Toggle word highlighting on, and verify word_highlight pref is enabled.
    wordHighlightToggle.click();
    const wordHighlightPref = await new Promise(
        resolve => chrome.settingsPrivate.getPref(
            'settings.a11y.select_to_speak_word_highlight', pref => {
              resolve(pref);
            }));
    assertTrue(wordHighlightPref.value);
  });
});
