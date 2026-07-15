// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://iwa-dev/app.js';

import type {IwaDevAppElement} from 'chrome://iwa-dev/app.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('<iwa-dev-app>', () => {
  let app: IwaDevAppElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    app = document.createElement('iwa-dev-app');
    document.body.appendChild(app);
  });

  test('display error message when IWA dev mode is disabled', async () => {
    app.devModeEnabled = false;
    await microtasksFinished();

    const heading = app.shadowRoot.querySelector('h1');
    assertTrue(!!heading);
    assertEquals('Isolated Web App Developer Tool', heading.textContent.trim());

    const errorMessage = app.shadowRoot.querySelector('#error-message');
    assertTrue(!!errorMessage);
    assertTrue(errorMessage.textContent.includes(
        'Isolated Web App Developer Mode is disabled.'));
    const link = errorMessage.querySelector('a');
    assertTrue(!!link);
    assertEquals('chrome://flags/#enable-isolated-web-app-dev-mode', link.href);
  });

  test('display content when IWA dev mode is enabled', async () => {
    app.devModeEnabled = true;
    await microtasksFinished();

    const heading = app.shadowRoot.querySelector('h1');
    assertTrue(!!heading);
    assertEquals('Isolated Web App Developer Tool', heading.textContent.trim());

    assertFalse(!!app.shadowRoot.querySelector('#error-message'));
  });
});
