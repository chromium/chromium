// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {SeaPenTemplatesElement, WallpaperGridItemElement} from 'chrome://personalization/js/personalization_app.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';

suite('SeaPenTemplatesElementTest', function() {
  let seaPenTemplatesElement: SeaPenTemplatesElement|null;

  setup(() => {
    loadTimeData.overrideValues(
        {isSeaPenEnabled: true, isSeaPenTextInputEnabled: false});
    baseSetup();
  });

  teardown(async () => {
    await teardownElement(seaPenTemplatesElement);
    seaPenTemplatesElement = null;
  });

  test('show template info on hover', async () => {
    seaPenTemplatesElement = initElement(SeaPenTemplatesElement);
    await waitAfterNextRender(seaPenTemplatesElement);

    const templates = seaPenTemplatesElement.shadowRoot!.querySelectorAll<
        WallpaperGridItemElement>(
        `${WallpaperGridItemElement.is}[data-sea-pen-image]:not([hidden])`);
    assertEquals(8, templates.length, 'there are 8 templates');
    await waitAfterNextRender(templates[7]!);
    assertEquals(templates[7]!.innerText, 'Airbrushed');
    templates[7]!.dispatchEvent(new CustomEvent('mouseover', {bubbles: true}));
    await waitAfterNextRender(templates[7]!);
    assertEquals(templates[7]!.innerText, 'A radiant\npink\norchid');
  });
});
