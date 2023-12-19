// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {SeaPenInputQueryElement, SeaPenPaths} from 'chrome://personalization/js/personalization_app.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {initElement, teardownElement} from './personalization_app_test_utils.js';

suite('SeaPenInputQueryElementTest', function() {
  let seaPenInputQueryElement: SeaPenInputQueryElement|null;

  setup(function() {
    loadTimeData.overrideValues({isSeaPenTextInputEnabled: true});
  });

  teardown(async () => {
    await teardownElement(seaPenInputQueryElement);
    seaPenInputQueryElement = null;
  });

  test('displays search buttonon root page', async () => {
    seaPenInputQueryElement =
        initElement(SeaPenInputQueryElement, {path: SeaPenPaths.ROOT});
    await waitAfterNextRender(seaPenInputQueryElement);

    const searchButton = seaPenInputQueryElement.shadowRoot!.querySelector(
                             '#searchButton') as HTMLElement;

    assertEquals(
        seaPenInputQueryElement.i18n('seaPenCreateButton'),
        searchButton!.innerText);
  });

  test('displays search again button on results page', async () => {
    seaPenInputQueryElement =
        initElement(SeaPenInputQueryElement, {path: SeaPenPaths.RESULTS});
    await waitAfterNextRender(seaPenInputQueryElement);

    const searchButton = seaPenInputQueryElement.shadowRoot!.querySelector(
                             '#searchButton') as HTMLElement;

    assertEquals(
        seaPenInputQueryElement.i18n('seaPenRecreateButton'),
        searchButton!.innerText);
  });
});
