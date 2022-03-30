// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {PersonalizationActionName, PersonalizationToastElement} from 'chrome://personalization/trusted/personalization_app.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/test_util.js';

import {baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';

suite('PersonalizationToastTest', function() {
  let personalizationToastElement: PersonalizationToastElement;

  let personalizationStore: TestPersonalizationStore;

  setup(() => {
    const mocks = baseSetup();
    personalizationStore = mocks.personalizationStore;
    personalizationToastElement = initElement(PersonalizationToastElement);
  });

  teardown(async () => {
    await teardownElement(personalizationToastElement);
    await flushTasks();
  });

  test('hidden when no error is present', async () => {
    assertEquals('', personalizationToastElement.innerHTML);
  });

  test('visible when error is present', async () => {
    personalizationStore.data.error = 'There was an error';
    personalizationStore.notifyObservers();
    await waitAfterNextRender(personalizationToastElement);
    assertTrue(
        !!personalizationToastElement.shadowRoot!.getElementById('container'));
    assertEquals(
        personalizationStore.data.error,
        personalizationToastElement.shadowRoot!.querySelector('p')!.innerText);
  });

  test('deploys an dismiss action when dismiss is clicked', async () => {
    personalizationStore.data.error = 'There was an error';
    personalizationStore.notifyObservers();
    await waitAfterNextRender(personalizationToastElement);

    personalizationStore.expectAction(PersonalizationActionName.DISMISS_ERROR);
    personalizationToastElement.shadowRoot!.querySelector('cr-button')!.click();
    await personalizationStore.waitForAction(
        PersonalizationActionName.DISMISS_ERROR);
  });
});
