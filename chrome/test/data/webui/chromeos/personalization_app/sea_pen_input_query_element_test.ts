// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {SeaPenInputQueryElement, SeaPenPaths} from 'chrome://personalization/js/personalization_app.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestPersonalizationStore} from 'test_personalization_store.js';
import {TestSeaPenProvider} from 'test_sea_pen_interface_provider.js';

import {baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';

suite('SeaPenInputQueryElementTest', function() {
  let seaPenInputQueryElement: SeaPenInputQueryElement|null;
  let personalizationStore: TestPersonalizationStore;
  let seaPenProvider: TestSeaPenProvider;

  setup(function() {
    loadTimeData.overrideValues({isSeaPenTextInputEnabled: true});
    const mocks = baseSetup();
    personalizationStore = mocks.personalizationStore;
    seaPenProvider = mocks.seaPenProvider;
  });

  teardown(async () => {
    await teardownElement(seaPenInputQueryElement);
    seaPenInputQueryElement = null;
  });

  test('displays search button on root page', async () => {
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
    personalizationStore.data.wallpaper.seaPen.thumbnails =
        seaPenProvider.images;
    seaPenInputQueryElement =
        initElement(SeaPenInputQueryElement, {path: SeaPenPaths.RESULTS});
    await waitAfterNextRender(seaPenInputQueryElement);

    const searchButton =
        seaPenInputQueryElement.shadowRoot!.querySelector<HTMLElement>(
            '#searchButton');
    const icon = searchButton!.querySelector<HTMLElement>('iron-icon');
    assertEquals(
        seaPenInputQueryElement.i18n('seaPenRecreateButton'),
        searchButton!.innerText);
    assertEquals('personalization-shared:refresh', icon!.getAttribute('icon'));
  });

  test('displays create button when no thumbnails are generated', async () => {
    seaPenInputQueryElement =
        initElement(SeaPenInputQueryElement, {path: SeaPenPaths.RESULTS});
    await waitAfterNextRender(seaPenInputQueryElement);

    const searchButton =
        seaPenInputQueryElement.shadowRoot!.querySelector<HTMLElement>(
            '#searchButton');
    const icon = searchButton!.querySelector<HTMLElement>('iron-icon');
    assertEquals(
        seaPenInputQueryElement.i18n('seaPenCreateButton'),
        searchButton!.innerText);
    assertEquals('sea-pen:photo-spark', icon!.getAttribute('icon'));
  });
});
