// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for theme-element component.  */

import 'chrome://personalization/strings.m.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {emptyState, PersonalizationThemeElement, SetDarkModeEnabledAction, ThemeActionName, ThemeObserver} from 'chrome://personalization/js/personalization_app.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {baseSetup, initElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';
import {TestThemeProvider} from './test_theme_interface_provider.js';

suite('PersonalizationThemeTest', function() {
  let personalizationThemeElement: PersonalizationThemeElement|null;
  let themeProvider: TestThemeProvider;
  let personalizationStore: TestPersonalizationStore;

  setup(() => {
    const mocks = baseSetup();
    themeProvider = mocks.themeProvider;
    personalizationStore = mocks.personalizationStore;
    ThemeObserver.initThemeObserverIfNeeded();
  });

  teardown(async () => {
    if (personalizationThemeElement) {
      personalizationThemeElement.remove();
    }
    personalizationThemeElement = null;
    ThemeObserver.shutdown();
    await flushTasks();
  });

  test('displays content', async () => {
    personalizationStore.data.theme.darkModeEnabled = false;
    personalizationStore.data.theme.colorModeAutoScheduleEnabled = false;
    personalizationThemeElement = initElement(PersonalizationThemeElement);
    await waitAfterNextRender(personalizationThemeElement);

    assertEquals(
        personalizationThemeElement.i18n('themeLabel'),
        personalizationThemeElement.shadowRoot!.querySelector('h2')!.innerText);
  });

  test('sets color mode in store on first load', async () => {
    personalizationStore.expectAction(ThemeActionName.SET_DARK_MODE_ENABLED);
    personalizationThemeElement = initElement(PersonalizationThemeElement);
    const action =
        await personalizationStore.waitForAction(
            ThemeActionName.SET_DARK_MODE_ENABLED) as SetDarkModeEnabledAction;
    assertTrue(action.enabled);
  });

  test('sets theme data in store on changed', async () => {
    // Make sure state starts as expected.
    assertDeepEquals(emptyState(), personalizationStore.data);
    await themeProvider.whenCalled('setThemeObserver');

    personalizationStore.expectAction(ThemeActionName.SET_DARK_MODE_ENABLED);
    themeProvider.themeObserverRemote!.onColorModeChanged(
        /*darkModeEnabled=*/ false);

    const {enabled} =
        await personalizationStore.waitForAction(
            ThemeActionName.SET_DARK_MODE_ENABLED) as SetDarkModeEnabledAction;
    assertFalse(enabled);
  });

  test('shows pressed button on load', async () => {
    personalizationThemeElement = initElement(PersonalizationThemeElement);
    personalizationStore.data.theme.darkModeEnabled = true;
    personalizationStore.data.theme.colorModeAutoScheduleEnabled = false;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(personalizationThemeElement);
    const radioButton =
        personalizationThemeElement.shadowRoot!.getElementById('darkMode');
    assertTrue(!!radioButton);
    assertEquals(radioButton.getAttribute('aria-pressed'), 'true');
  });

  test('sets dark mode enabled when dark button is clicked', async () => {
    personalizationThemeElement = initElement(PersonalizationThemeElement);
    personalizationStore.data.theme.darkModeEnabled = false;
    personalizationStore.data.theme.colorModeAutoScheduleEnabled = false;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(personalizationThemeElement);
    const radioButton =
        personalizationThemeElement.shadowRoot!.getElementById('darkMode');
    assertTrue(!!radioButton);
    assertEquals(radioButton.getAttribute('aria-pressed'), 'false');

    personalizationStore.setReducersEnabled(true);
    personalizationStore.expectAction(ThemeActionName.SET_DARK_MODE_ENABLED);
    radioButton.click();
    const action =
        await personalizationStore.waitForAction(
            ThemeActionName.SET_DARK_MODE_ENABLED) as SetDarkModeEnabledAction;
    assertTrue(action.enabled);
    assertTrue(personalizationStore.data.theme.darkModeEnabled);
    assertEquals(radioButton.getAttribute('aria-pressed'), 'true');
  });

  test('sets auto mode enabled when auto button is clicked', async () => {
    personalizationThemeElement = initElement(PersonalizationThemeElement);
    personalizationStore.data.theme.darkModeEnabled = false;
    personalizationStore.data.theme.colorModeAutoScheduleEnabled = false;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(personalizationThemeElement);
    const radioButton =
        personalizationThemeElement.shadowRoot!.getElementById('autoMode');
    assertTrue(!!radioButton);
    assertEquals(radioButton.getAttribute('aria-pressed'), 'false');

    personalizationStore.setReducersEnabled(true);
    personalizationStore.expectAction(
        ThemeActionName.SET_COLOR_MODE_AUTO_SCHEDULE_ENABLED);
    radioButton.click();
    const action = await personalizationStore.waitForAction(
                       ThemeActionName.SET_COLOR_MODE_AUTO_SCHEDULE_ENABLED) as
        SetDarkModeEnabledAction;
    assertTrue(action.enabled);
    assertTrue(personalizationStore.data.theme.colorModeAutoScheduleEnabled);
    assertEquals(radioButton.getAttribute('aria-pressed'), 'true');

    // reclicking the button does not disable auto mode.
    radioButton.click();
    assertEquals(radioButton.getAttribute('aria-pressed'), 'true');
  });
});
