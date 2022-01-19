// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for theme-element component.  */

import {emptyState} from 'chrome://personalization/trusted/personalization_state.js';
import {PersonalizationThemeElement} from 'chrome://personalization/trusted/personalization_theme_element.js';
import {ThemeActionName} from 'chrome://personalization/trusted/theme/theme_actions.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/test_util.js';

import {baseSetup, initElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';
import {TestThemeProvider} from './test_theme_interface_provider.js';

export function PersonalizationThemeTest() {
  let personalizationThemeElement: PersonalizationThemeElement|null;
  let themeProvider: TestThemeProvider;
  let personalizationStore: TestPersonalizationStore;

  setup(() => {
    const mocks = baseSetup();
    themeProvider = mocks.themeProvider;
    personalizationStore = mocks.personalizationStore;
  });

  teardown(async () => {
    if (personalizationThemeElement) {
      personalizationThemeElement.remove();
    }
    personalizationThemeElement = null;
    await flushTasks();
  });

  test('displays content', async () => {
    personalizationStore.data.theme = {darkModeEnabled: false};
    personalizationThemeElement = initElement(PersonalizationThemeElement);
    await waitAfterNextRender(personalizationThemeElement);

    assertEquals(
        personalizationThemeElement.i18n('themeLabel'),
        personalizationThemeElement.shadowRoot!.querySelector('h2')!.innerText);
  });

  test('sets color mode in store on first load', async () => {
    personalizationStore.expectAction(ThemeActionName.SET_DARK_MODE_ENABLED);
    personalizationThemeElement = initElement(PersonalizationThemeElement);
    const action = await personalizationStore.waitForAction(
        ThemeActionName.SET_DARK_MODE_ENABLED);
    assertTrue(action.enabled);
  });

  test('sets theme data in store on changed', async () => {
    // Make sure state starts as expected.
    assertDeepEquals(emptyState(), personalizationStore.data);

    personalizationThemeElement = initElement(PersonalizationThemeElement);

    await themeProvider.whenCalled('setThemeObserver');

    personalizationStore.expectAction(ThemeActionName.SET_DARK_MODE_ENABLED);
    themeProvider.themeObserverRemote!.onColorModeChanged(
        /*darkModeEnabled=*/ false);

    const {enabled} = await personalizationStore.waitForAction(
        ThemeActionName.SET_DARK_MODE_ENABLED);
    assertFalse(enabled);
  });

  test('shows pressed button on load', async () => {
    personalizationThemeElement = initElement(PersonalizationThemeElement);
    personalizationStore.data.theme.darkModeEnabled = true;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(personalizationThemeElement);
    const radioButton =
        personalizationThemeElement.shadowRoot!.getElementById('darkMode');
    assertTrue(!!radioButton);
    assertEquals(radioButton.getAttribute('aria-pressed'), 'true');
  });
}
