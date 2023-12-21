// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://compose/app.js';

import {ComposeAppElement} from 'chrome://compose/app.js';
import {ComposeApiProxyImpl} from 'chrome://compose/compose_api_proxy.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestComposeApiProxy} from './test_compose_api_proxy.js';

suite('ComposeApp', function() {
  let testProxy: TestComposeApiProxy;

  async function createApp(): Promise<ComposeAppElement> {
    const app = document.createElement('compose-app');
    document.body.appendChild(app);

    await testProxy.whenCalled('requestInitialState');
    await flushTasks();
    return app;
  }

  setup(async () => {
    testProxy = new TestComposeApiProxy();
    ComposeApiProxyImpl.setInstance(testProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  test('RefocusesInputOnInvalidate', async () => {
    const app = await createApp();
    app.$.textarea.value = 'short';
    app.$.textarea.dispatchEvent(new CustomEvent('value-changed'));
    app.$.submitButton.focus();
    app.$.submitButton.click();
    await flushTasks();
    assertEquals(app.$.textarea, app.shadowRoot!.activeElement);
  });

  test('FocusesEditInput', async () => {
    testProxy.setOpenMetadata({}, {
      webuiState: JSON.stringify({
        input: 'some input',
        isEditingSubmittedInput: true,
      }),
    });
    const app = await createApp();
    assertEquals(app.$.editTextarea, app.shadowRoot!.activeElement);
  });
});
