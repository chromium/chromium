// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {OsSettingsSmartInputsPageElement} from 'chrome://os-settings/lazy_load.js';
import {Router, routes} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getDeepActiveElement} from 'chrome://resources/js/util_ts.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';


suite('<os-settings-smart-inputs-page>', function() {
  let smartInputsPage: OsSettingsSmartInputsPageElement;

  function createSmartInputsPage() {
    smartInputsPage = document.createElement('os-settings-smart-inputs-page');
    document.body.appendChild(smartInputsPage);
    flush();
  }

  teardown(function() {
    smartInputsPage.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('emojiSuggestAdditionNotNullWhenAllowEmojiSuggestionIsTrue', function() {
    loadTimeData.overrideValues({allowEmojiSuggestion: true});
    createSmartInputsPage();
    assert(smartInputsPage.shadowRoot!.querySelector('#emojiSuggestion'));
  });

  test('emojiSuggestAdditionNullWhenAllowEmojiSuggestionIsFalse', function() {
    loadTimeData.overrideValues({allowEmojiSuggestion: false});
    createSmartInputsPage();
    assertEquals(
        null, smartInputsPage.shadowRoot!.querySelector('#emojiSuggestion'));
  });

  test('Deep link to emoji suggestion toggle', async () => {
    loadTimeData.overrideValues({
      allowEmojiSuggestion: true,
    });
    createSmartInputsPage();

    const params = new URLSearchParams();
    params.append('settingId', '1203');
    Router.getInstance().navigateTo(routes.OS_LANGUAGES_SMART_INPUTS, params);

    flush();

    const deepLinkElement =
        smartInputsPage.shadowRoot!.querySelector('#emojiSuggestion')!
            .shadowRoot!.querySelector('cr-toggle');
    assert(deepLinkElement);
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Emoji suggestion toggle should be focused for settingId=1203.');
  });

  suite('Orca setting toggle', () => {
    test('should appear if allowOrca flag is true.', () => {
      loadTimeData.overrideValues({
        allowOrca: true,
      });
      createSmartInputsPage();
      assert(smartInputsPage.shadowRoot!.querySelector('#orca'));
    });

    test('should be hidden if allowOrca flag is false.', () => {
      loadTimeData.overrideValues({
        allowOrca: false,
      });
      createSmartInputsPage();
      assertEquals(null, smartInputsPage.shadowRoot!.querySelector('#orca'));
    });
  });
});
