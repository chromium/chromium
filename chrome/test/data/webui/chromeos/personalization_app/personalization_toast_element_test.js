// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PersonalizationActionName} from 'chrome://personalization/trusted/personalization_actions.js';
import {PersonalizationToastElement} from 'chrome://personalization/trusted/personalization_toast_element.js';

import {assertEquals, assertTrue} from '../../chai_assert.js';
import {flushTasks, waitAfterNextRender} from '../../test_util.js';

import {baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';

export function PersonalizationToastTest() {
  /** @type {!HTMLElement} */
  let personalizationToastElement;

  /** @type {?TestPersonalizationStore} */
  let personalizationStore = null;

  setup(() => {
    const mocks = baseSetup();
    personalizationStore = mocks.personalizationStore;
    personalizationToastElement = initElement(PersonalizationToastElement.is);
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
        !!personalizationToastElement.shadowRoot.getElementById('container'));
    assertEquals(
        personalizationStore.data.error,
        personalizationToastElement.shadowRoot.querySelector('p').innerText);
  });

  test('deploys an dismiss action when dismiss is clicked', async () => {
    personalizationStore.data.error = 'There was an error';
    personalizationStore.notifyObservers();
    await waitAfterNextRender(personalizationToastElement);

    personalizationStore.expectAction(PersonalizationActionName.DISMISS_ERROR);
    personalizationToastElement.shadowRoot.querySelector('cr-button').click();
    await personalizationStore.waitForAction(
        PersonalizationActionName.DISMISS_ERROR);
  });
}
