// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {FaceGazeActionsCardElement} from 'chrome://os-settings/lazy_load.js';
import {CrSettingsPrefs, Router, routes, SettingsPrefsElement, SettingsToggleButtonElement} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {clearBody} from '../utils.js';

suite('<facegaze-actions-card>', () => {
  let faceGazeActionsCard: FaceGazeActionsCardElement;
  let prefElement: SettingsPrefsElement;

  async function initPage() {
    prefElement = document.createElement('settings-prefs');
    document.body.appendChild(prefElement);

    await CrSettingsPrefs.initialized;
    faceGazeActionsCard = document.createElement('facegaze-actions-card');
    faceGazeActionsCard.prefs = prefElement.prefs;
    document.body.appendChild(faceGazeActionsCard);
    flush();
  }

  setup(() => {
    clearBody();
    Router.getInstance().navigateTo(routes.MANAGE_FACEGAZE_SETTINGS);
  });

  teardown(() => {
    faceGazeActionsCard.remove();
    prefElement.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('actions enabled button syncs to pref', async () => {
    await initPage();
    assertTrue(faceGazeActionsCard.prefs.settings.a11y.face_gaze.actions_enabled
                   .value);

    const button = faceGazeActionsCard.shadowRoot!
                       .querySelector<SettingsToggleButtonElement>(
                           '#faceGazeActionsEnabledButton');
    assert(button);
    assertTrue(isVisible(button));
    assertTrue(button.checked);

    button.click();
    flush();

    assertFalse(button.checked);
    assertFalse(faceGazeActionsCard.prefs.settings.a11y.face_gaze
                    .actions_enabled.value);
  });
});
