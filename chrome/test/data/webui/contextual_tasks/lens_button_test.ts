// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/contextual_tasks_extension/lens_button_app.js';
import 'chrome://contextual-tasks/strings.m.js';

import type {LensButtonAppElement} from 'chrome://contextual-tasks/contextual_tasks_extension/lens_button_app.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('LensButtonTest', () => {
  let app: LensButtonAppElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    loadTimeData.overrideValues({
      lensSearchButtonLabel: 'Search with Google Lens',
    });

    app = document.createElement('lens-button-app');
    document.body.appendChild(app);
    await microtasksFinished();
  });

  test('Button renders and is visible', () => {
    assertTrue(isVisible(app));
    const button = app.$.lensButton;
    assertTrue(isVisible(button));
    assertEquals('Search with Google Lens', button.getAttribute('aria-label'));
    assertEquals('Search with Google Lens', button.getAttribute('title'));
  });

  test('Supports active and disabled states', async () => {
    const button = app.$.lensButton;
    assertFalse(app.active);
    assertFalse(button.hasAttribute('active'));

    app.active = true;
    await microtasksFinished();
    assertTrue(app.hasAttribute('active'));
    assertTrue(button.hasAttribute('active'));

    assertFalse(app.disabled);
    assertFalse(button.disabled);

    app.disabled = true;
    await microtasksFinished();
    assertTrue(app.hasAttribute('disabled'));
    assertTrue(button.disabled);
  });

  test('Handles missing label gracefully', async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    loadTimeData.overrideValues({
      lensSearchButtonLabel: '',
    });

    const testApp = document.createElement('lens-button-app');
    document.body.appendChild(testApp);
    await microtasksFinished();

    const button = testApp.$.lensButton;
    assertEquals('', button.getAttribute('aria-label'));
    assertEquals('', button.getAttribute('title'));
  });
});
