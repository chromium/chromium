// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SeaPenToastElement} from 'chrome://personalization/js/personalization_app.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';

suite('SeaPenToastTest', function() {
  let seaPenToastElement: SeaPenToastElement;

  let personalizationStore: TestPersonalizationStore;

  setup(() => {
    const mocks = baseSetup();
    personalizationStore = mocks.personalizationStore;
    seaPenToastElement = initElement(SeaPenToastElement);
  });

  teardown(async () => {
    await teardownElement(seaPenToastElement);
    await flushTasks();
  });

  test('displays error message', async () => {
    const errorMessage = 'test error message';
    personalizationStore.data.wallpaper.seaPen = {
      ...personalizationStore.data.wallpaper.seaPen,
      error: errorMessage,
    };
    personalizationStore.notifyObservers();
    await waitAfterNextRender(seaPenToastElement);
    assertEquals(
        errorMessage,
        seaPenToastElement.shadowRoot?.querySelector('p')?.textContent);
  });

  test('clicking dismiss resets error message', async () => {
    personalizationStore.setReducersEnabled(true);

    const errorMessage = 'test error message';
    personalizationStore.data.wallpaper.seaPen = {
      ...personalizationStore.data.wallpaper.seaPen,
      error: errorMessage,
    };
    personalizationStore.notifyObservers();
    await waitAfterNextRender(seaPenToastElement);

    seaPenToastElement.shadowRoot?.querySelector('cr-button')?.click();

    await waitAfterNextRender(seaPenToastElement);

    assertEquals(
        'none',
        getComputedStyle(
            seaPenToastElement.shadowRoot?.getElementById('container')!)
            .display);
  });
});
