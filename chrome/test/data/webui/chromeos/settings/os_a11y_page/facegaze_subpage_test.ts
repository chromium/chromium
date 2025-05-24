// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import type {SettingsFaceGazeSubpageElement} from 'chrome://os-settings/lazy_load.js';
import type {SettingsCardElement, SettingsPrefsElement, SettingsToggleButtonElement} from 'chrome://os-settings/os_settings.js';
import {CrSettingsPrefs, FaceGazeSubpageBrowserProxyImpl, Router, routes} from 'chrome://os-settings/os_settings.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {clearBody} from '../utils.js';

import {TestFaceGazeSubpageBrowserProxy} from './test_facegaze_subpage_browser_proxy.js';

suite('<settings-facegaze-subpage>', () => {
  let browserProxy: TestFaceGazeSubpageBrowserProxy;
  let faceGazeSubpage: SettingsFaceGazeSubpageElement;
  let prefElement: SettingsPrefsElement;

  function getToggleButton(): SettingsToggleButtonElement {
    const toggle =
        faceGazeSubpage.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#faceGazeToggle');
    assertTrue(!!toggle);
    assertTrue(isVisible(toggle));
    return toggle;
  }

  async function initPage() {
    prefElement = document.createElement('settings-prefs');
    document.body.appendChild(prefElement);

    await CrSettingsPrefs.initialized;
    faceGazeSubpage = document.createElement('settings-facegaze-subpage');
    faceGazeSubpage.prefs = prefElement.prefs;
    document.body.appendChild(faceGazeSubpage);
    flush();
  }

  async function toggleFaceGazeOnUsingToggle() {
    // When toggling the button on, a request will be sent to the browser to
    // enable FaceGaze. Since this isn't an integration test, we need to
    // manually set the preference value to reflect the new state.
    const toggle = getToggleButton();
    assertFalse(toggle.checked);

    toggle.click();
    faceGazeSubpage.setPrefValue('settings.a11y.face_gaze.enabled', true);
    await flushTasks();
  }

  async function toggleFaceGazeOffUsingToggleUsingToggle() {
    // When toggling the button off, a request will be sent to the browser to
    // show a dialog that asks the user to confirm if they'd like to turn off
    // FaceGaze. This temporarily turns the toggle off.
    const toggle = getToggleButton();
    assertTrue(toggle.checked);

    toggle.click();
    await flushTasks();
  }

  async function simulateFaceGazeDisableDialogAccepted() {
    // If the user accepts the dialog, then FaceGaze gets turned off by the
    // browser and the browser will notify this page that the dialog was
    // accepted.
    webUIListenerCallback('settings.handleDisableDialogResult', true);
    faceGazeSubpage.setPrefValue('settings.a11y.face_gaze.enabled', false);
    await flushTasks();
  }

  async function simulateFaceGazeDisableDialogCanceled() {
    webUIListenerCallback('settings.handleDisableDialogResult', false);
    await flushTasks();
  }

  setup(() => {
    browserProxy = new TestFaceGazeSubpageBrowserProxy();
    FaceGazeSubpageBrowserProxyImpl.setInstanceForTesting(browserProxy);
    clearBody();
    Router.getInstance().navigateTo(routes.MANAGE_FACEGAZE_SETTINGS);
  });

  teardown(() => {
    Router.getInstance().resetRouteForTesting();
    browserProxy.reset();
  });

  test('subpage contains three settings cards', async () => {
    await initPage();

    // Page should contain cards for the feature toggle button, the cursor
    // settings, and the action settings.
    const toggleCard =
        faceGazeSubpage.shadowRoot!.querySelector<SettingsCardElement>(
            'settings-card');
    assertTrue(!!toggleCard);

    const cursorCard =
        faceGazeSubpage.shadowRoot!.querySelector('facegaze-cursor-card');
    assertTrue(!!cursorCard);
    const cursorSettingsCard =
        cursorCard.shadowRoot!.querySelector<SettingsCardElement>(
            'settings-card');
    assertTrue(!!cursorSettingsCard);

    const actionsCard =
        faceGazeSubpage.shadowRoot!.querySelector('facegaze-actions-card');
    assertTrue(!!actionsCard);
    const actionsSettingsCard =
        actionsCard.shadowRoot!.querySelector<SettingsCardElement>(
            'settings-card');
    assertTrue(!!actionsSettingsCard);
  });

  test('changing pref updates label and checked value', async () => {
    await initPage();

    const toggle = getToggleButton();
    assertFalse(toggle.checked);
    assertEquals('Off', toggle.label);

    faceGazeSubpage.setPrefValue('settings.a11y.face_gaze.enabled', true);
    await flushTasks();

    assertTrue(toggle.checked);
    assertEquals('On', toggle.label);

    faceGazeSubpage.setPrefValue('settings.a11y.face_gaze.enabled', false);
    await flushTasks();

    assertFalse(toggle.checked);
    assertEquals('Off', toggle.label);
  });

  test(
      'clicking toggle button calls API to change FaceGaze enabled state',
      async () => {
        await initPage();

        assertEquals(0, browserProxy.getCallCount('requestEnableFaceGaze'));

        const toggle = getToggleButton();
        assertFalse(toggle.checked);

        toggle.click();
        await flushTasks();

        assertEquals(1, browserProxy.getCallCount('requestEnableFaceGaze'));
        assertTrue(browserProxy.getArgs('requestEnableFaceGaze')[0][0]);

        toggle.click();
        await flushTasks();

        assertEquals(2, browserProxy.getCallCount('requestEnableFaceGaze'));
        assertFalse(browserProxy.getArgs('requestEnableFaceGaze')[1][0]);
      });

  test('toggle on, toggle off, and cancel dialog', async () => {
    await initPage();

    await toggleFaceGazeOnUsingToggle();
    assertTrue(getToggleButton().checked);

    await toggleFaceGazeOffUsingToggleUsingToggle();
    assertFalse(getToggleButton().checked);

    await simulateFaceGazeDisableDialogCanceled();
    assertTrue(getToggleButton().checked);
  });

  test('toggle on, toggle off, and accept dialog', async () => {
    await initPage();

    await toggleFaceGazeOnUsingToggle();
    assertTrue(getToggleButton().checked);

    await toggleFaceGazeOffUsingToggleUsingToggle();
    assertFalse(getToggleButton().checked);

    await simulateFaceGazeDisableDialogAccepted();
    assertFalse(getToggleButton().checked);
  });
});
