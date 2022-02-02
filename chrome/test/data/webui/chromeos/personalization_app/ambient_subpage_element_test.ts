// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


import {AmbientActionName, SetAmbientModeEnabledAction} from 'chrome://personalization/trusted/ambient/ambient_actions.js';
import {AmbientObserver} from 'chrome://personalization/trusted/ambient/ambient_observer.js';
import {AmbientSubpage} from 'chrome://personalization/trusted/ambient/ambient_subpage_element.js';
import {ToggleRowElement} from 'chrome://personalization/trusted/ambient/toggle_row.js';
import {emptyState} from 'chrome://personalization/trusted/personalization_state.js';
import {CrToggleElement} from 'chrome://resources/cr_elements/cr_toggle/cr_toggle.m.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/test_util.js';

import {baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestAmbientProvider} from './test_ambient_interface_provider.js';
import {TestPersonalizationStore} from './test_personalization_store.js';

export function AmbientSubpageTest() {
  let ambientSubpageElement: AmbientSubpage|null;
  let ambientProvider: TestAmbientProvider;
  let personalizationStore: TestPersonalizationStore;

  setup(() => {
    const mocks = baseSetup();
    ambientProvider = mocks.ambientProvider;
    personalizationStore = mocks.personalizationStore;
    AmbientObserver.initAmbientObserverIfNeeded();
  });

  teardown(async () => {
    await teardownElement(ambientSubpageElement);
    ambientSubpageElement = null;
    AmbientObserver.shutdown();
  });

  test('displays content', async () => {
    ambientSubpageElement = initElement(AmbientSubpage);
    assertEquals(
        'Ambient',
        ambientSubpageElement.shadowRoot!.querySelector('h2')!.innerText);

    const toggleRow =
        ambientSubpageElement.shadowRoot!.querySelector('toggle-row');
    assertTrue(!!toggleRow);
    const toggleButton =
        toggleRow!.shadowRoot!.querySelector('cr-toggle') as CrToggleElement;
    assertTrue(!!toggleButton);
    assertFalse(toggleButton!.checked);

    const topicSource =
        ambientSubpageElement.shadowRoot!.querySelector('topic-source-list');
    assertTrue(!!topicSource);
  });

  test('sets ambient mode enabled in store on first load', async () => {
    personalizationStore.expectAction(
        AmbientActionName.SET_AMBIENT_MODE_ENABLED);
    ambientSubpageElement = initElement(AmbientSubpage);
    const action = await personalizationStore.waitForAction(
                       AmbientActionName.SET_AMBIENT_MODE_ENABLED) as
        SetAmbientModeEnabledAction;
    assertTrue(action.enabled);
  });

  test('sets ambient mode when pref value changed', async () => {
    // Make sure state starts as expected.
    assertDeepEquals(emptyState(), personalizationStore.data);
    ambientSubpageElement = initElement(AmbientSubpage);
    await ambientProvider.whenCalled('setAmbientObserver');

    personalizationStore.expectAction(
        AmbientActionName.SET_AMBIENT_MODE_ENABLED);
    ambientProvider.ambientObserverRemote!.onAmbientModeEnabledChanged(
        /*ambientModeEnabled=*/ false);

    const action = await personalizationStore.waitForAction(
                       AmbientActionName.SET_AMBIENT_MODE_ENABLED) as
        SetAmbientModeEnabledAction;
    assertFalse(action.enabled);
  });

  test('sets ambient mode enabled when toggle row clicked', async () => {
    ambientSubpageElement = initElement(AmbientSubpage);
    personalizationStore.data.ambient.ambientModeEnabled = true;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(ambientSubpageElement);

    const toggleRow = ambientSubpageElement.shadowRoot!.querySelector(
                          'toggle-row') as ToggleRowElement;
    assertTrue(!!toggleRow);
    let toggleButton =
        toggleRow!.shadowRoot!.querySelector('cr-toggle') as CrToggleElement;
    assertTrue(!!toggleButton);
    assertTrue(toggleButton!.checked);

    personalizationStore.setReducersEnabled(true);
    personalizationStore.expectAction(
        AmbientActionName.SET_AMBIENT_MODE_ENABLED);
    toggleRow.click();
    let action = await personalizationStore.waitForAction(
                     AmbientActionName.SET_AMBIENT_MODE_ENABLED) as
        SetAmbientModeEnabledAction;
    assertFalse(action.enabled);
    assertFalse(personalizationStore.data.ambient.ambientModeEnabled);
    assertFalse(toggleButton!.checked);

    personalizationStore.expectAction(
        AmbientActionName.SET_AMBIENT_MODE_ENABLED);
    toggleRow.click();
    action = await personalizationStore.waitForAction(
                 AmbientActionName.SET_AMBIENT_MODE_ENABLED) as
        SetAmbientModeEnabledAction;
    assertTrue(action.enabled);
    assertTrue(personalizationStore.data.ambient.ambientModeEnabled);
    assertTrue(toggleButton!.checked);
  });

  test('sets ambient mode enabled when toggle button clicked', async () => {
    ambientSubpageElement = initElement(AmbientSubpage);
    personalizationStore.data.ambient.ambientModeEnabled = true;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(ambientSubpageElement);

    const toggleRow = ambientSubpageElement.shadowRoot!.querySelector(
                          'toggle-row') as ToggleRowElement;
    assertTrue(!!toggleRow);
    const toggleButton =
        toggleRow!.shadowRoot!.querySelector('cr-toggle') as CrToggleElement;
    assertTrue(!!toggleButton);
    assertTrue(toggleButton!.checked);

    personalizationStore.expectAction(
        AmbientActionName.SET_AMBIENT_MODE_ENABLED);
    toggleRow.$.toggle.click();
    let action = await personalizationStore.waitForAction(
                     AmbientActionName.SET_AMBIENT_MODE_ENABLED) as
        SetAmbientModeEnabledAction;
    assertFalse(action.enabled);

    personalizationStore.expectAction(
        AmbientActionName.SET_AMBIENT_MODE_ENABLED);
    toggleRow.$.toggle.click();
    action = await personalizationStore.waitForAction(
                 AmbientActionName.SET_AMBIENT_MODE_ENABLED) as
        SetAmbientModeEnabledAction;
    assertTrue(action.enabled);
  });
}
