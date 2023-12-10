// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {Paths, SeaPenInputQueryElement} from 'chrome://personalization/js/personalization_app.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {initElement, teardownElement} from './personalization_app_test_utils.js';

suite('SeaPenInputQueryElementTest', function() {
  let seaPenInputQueryElement: SeaPenInputQueryElement|null;

  teardown(async () => {
    await teardownElement(seaPenInputQueryElement);
    seaPenInputQueryElement = null;
  });

  test('displays sea pen input on collection page', async () => {
    seaPenInputQueryElement = initElement(
        SeaPenInputQueryElement, {'path': Paths.SEA_PEN_COLLECTION});
    await waitAfterNextRender(seaPenInputQueryElement);

    const searchButton = seaPenInputQueryElement.shadowRoot!.querySelector(
                             '#searchButton') as HTMLElement;

    assertEquals('Search', searchButton!.innerText);
  });

  test('displays search again button on results page', async () => {
    seaPenInputQueryElement =
        initElement(SeaPenInputQueryElement, {'path': Paths.SEA_PEN_RESULTS});
    await waitAfterNextRender(seaPenInputQueryElement);

    const searchButton = seaPenInputQueryElement.shadowRoot!.querySelector(
                             '#searchButton') as HTMLElement;

    assertEquals('Search again', searchButton!.innerText);
  });
});
