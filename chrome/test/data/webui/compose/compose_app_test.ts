// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://compose/app.js';

import {ComposeAppElement} from 'chrome://compose/app.js';
import {ComposeResponse, ComposeStatus, StyleModifiers} from 'chrome://compose/compose.mojom-webui.js';
import {ComposeApiProxy, ComposeApiProxyImpl} from 'chrome://compose/compose_api_proxy.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible, whenCheck} from 'chrome://webui-test/test_util.js';


class TestingApiProxy implements ComposeApiProxy {
  constructor() {}

  compose(_style: StyleModifiers, _input: string): Promise<ComposeResponse> {
    return Promise.resolve(
        {status: ComposeStatus.kOk, result: 'Here is my output.'});
  }
}

suite('ComposeApp', () => {
  let app: ComposeAppElement;

  setup(() => {
    ComposeApiProxyImpl.setInstance(new TestingApiProxy());

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    app = document.createElement('compose-app');
    document.body.appendChild(app);

    return flushTasks();
  });

  test('SubmitsInput', async () => {
    // Starts off with submit disabled since input is empty.
    assertTrue(isVisible(app.$.submitButton));
    assertTrue(app.$.submitButton.disabled);
    assertFalse(isVisible(app.$.resultContainer));
    assertFalse(isVisible(app.$.insertButton));

    // Invalid input keeps submit disabled.
    app.$.textarea.value = 'Short';
    app.$.textarea.dispatchEvent(new CustomEvent('value-changed'));
    assertTrue(app.$.submitButton.disabled);

    // Inputting valid text enables submit.
    app.$.textarea.value = 'Here is my input.';
    app.$.textarea.dispatchEvent(new CustomEvent('value-changed'));
    assertFalse(app.$.submitButton.disabled);

    // Clicking on submit gets results.
    app.$.submitButton.click();

    // Wait for api proxy promise to resolve.
    await flushTasks();

    const resultContainerVisible = whenCheck(app, () => {
      return !app.$.resultContainer.hidden;
    });
    await resultContainerVisible;

    assertFalse(isVisible(app.$.submitButton));
    assertTrue(app.$.textarea.readonly);
    assertTrue(isVisible(app.$.insertButton));
  });
});
