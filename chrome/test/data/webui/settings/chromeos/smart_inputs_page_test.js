// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/chromeos/lazy_load.js';

import {Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
import {getDeepActiveElement} from 'chrome://resources/ash/common/util.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

let smartInputsPage;

function createSmartInputsPage() {
  PolymerTest.clearBody();
  smartInputsPage = document.createElement('os-settings-smart-inputs-page');
  document.body.appendChild(smartInputsPage);
  flush();
}

suite('SmartInputsPage', function() {
  teardown(function() {
    smartInputsPage.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test(
      'assistPersonalInfoNotNullWhenAllowAssistivePersonalInfoIsTrue',
      function() {
        loadTimeData.overrideValues({allowAssistivePersonalInfo: true});
        createSmartInputsPage();
        assertTrue(
            !!smartInputsPage.shadowRoot.querySelector('#assistPersonalInfo'));
      });

  test(
      'assistPersonalInfoNullWhenAllowAssistivePersonalInfoIsFalse',
      function() {
        loadTimeData.overrideValues({allowAssistivePersonalInfo: false});
        createSmartInputsPage();
        assertFalse(
            !!smartInputsPage.shadowRoot.querySelector('#assistPersonalInfo'));
      });

  test('emojiSuggestAdditionNotNullWhenAllowEmojiSuggestionIsTrue', function() {
    loadTimeData.overrideValues({allowEmojiSuggestion: true});
    createSmartInputsPage();
    assertTrue(!!smartInputsPage.shadowRoot.querySelector('#emojiSuggestion'));
  });

  test('emojiSuggestAdditionNullWhenAllowEmojiSuggestionIsFalse', function() {
    loadTimeData.overrideValues({allowEmojiSuggestion: false});
    createSmartInputsPage();
    assertFalse(!!smartInputsPage.shadowRoot.querySelector('#emojiSuggestion'));
  });

  test('Deep link to emoji suggestion toggle', async () => {
    loadTimeData.overrideValues({
      allowEmojiSuggestion: true,
    });
    createSmartInputsPage();

    const params = new URLSearchParams();
    params.append('settingId', '1203');
    Router.getInstance().navigateTo(
        routes.OS_LANGUAGES_SMART_INPUTS, params);

    flush();

    const deepLinkElement =
        smartInputsPage.shadowRoot.querySelector('#emojiSuggestion')
            .shadowRoot.querySelector('cr-toggle');
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Emoji suggestion toggle should be focused for settingId=1203.');
  });
});
