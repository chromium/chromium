// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://personalization/strings.m.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {PersonalizationMain} from 'chrome://personalization/js/personalization_app.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';

suite('PersonalizationMainTest', function() {
  let personalizationMainElement: PersonalizationMain|null;

  setup(() => {
    baseSetup();
  });

  teardown(async () => {
    await teardownElement(personalizationMainElement);
    personalizationMainElement = null;
  });

  test('has large ambient preview when ambient allowed', async () => {
    loadTimeData.overrideValues({isAmbientModeAllowed: true});
    personalizationMainElement = initElement(PersonalizationMain);
    await waitAfterNextRender(personalizationMainElement);

    const preview = personalizationMainElement!.shadowRoot!.querySelector(
        'ambient-preview-large')!;
    assertTrue(!!preview, 'ambient preview exists');
  });

  test('has preview when ambient disallowed but jelly enabled', async () => {
    loadTimeData.overrideValues(
        {isAmbientModeAllowed: false, isPersonalizationJellyEnabled: true});
    personalizationMainElement = initElement(PersonalizationMain);
    await waitAfterNextRender(personalizationMainElement);

    const preview = personalizationMainElement!.shadowRoot!.querySelector(
        'ambient-preview-large')!;
    assertTrue(!!preview, 'ambient preview exists');
  });

  test('has no ambient preview when ambient and jelly disallowed', async () => {
    loadTimeData.overrideValues(
        {isAmbientModeAllowed: false, isPersonalizationJellyEnabled: false});
    personalizationMainElement = initElement(PersonalizationMain);
    await waitAfterNextRender(personalizationMainElement);

    const preview = personalizationMainElement!.shadowRoot!.querySelector(
        'ambient-preview-large')!;
    assertFalse(!!preview, 'ambient preview does not exist');
  });
});
