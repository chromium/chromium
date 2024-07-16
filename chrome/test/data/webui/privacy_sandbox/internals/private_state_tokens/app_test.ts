// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://privacy-sandbox-internals/private_state_tokens/private_state_tokens.js';

import type {PrivateStateTokensAppElement} from 'chrome://privacy-sandbox-internals/private_state_tokens/private_state_tokens.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('PrivateStateTokensAppTest', () => {
  let app: PrivateStateTokensAppElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    app = document.createElement('private-state-tokens-app');
    document.body.appendChild(app);
    app.setNarrowForTesting(false);
    await microtasksFinished();
  });

  test('check layout', () => {
    assertTrue(isVisible(app));
    assertTrue(isVisible(app.$.sidebar));
  });

  test('app drawer', async () => {
    app.setNarrowForTesting(true);
    await microtasksFinished();

    assertFalse(app.$.drawer.open);
    const menuButton =
        app.$.toolbar.$.mainToolbar.shadowRoot!.querySelector<HTMLElement>(
            '#menuButton');
    assertTrue(isVisible(menuButton));
    assertTrue(!!menuButton);
    menuButton.click();
    await microtasksFinished();

    assertTrue(app.$.drawer.open);
    app.$.drawer.close();
    await microtasksFinished();
    assertFalse(isVisible(app.$.drawer));
  });
});
