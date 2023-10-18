// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://compose/app.js';

import {ComposeAppElement} from 'chrome://compose/app.js';
import {ComposeDialogCallbackRouter, ComposeStatus, Length, StyleModifiers, Tone} from 'chrome://compose/compose.mojom-webui.js';
import {ComposeApiProxy, ComposeApiProxyImpl} from 'chrome://compose/compose_api_proxy.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {isVisible, whenCheck} from 'chrome://webui-test/test_util.js';


class TestingApiProxy extends TestBrowserProxy implements ComposeApiProxy {
  private composeOutput_: string = 'Some output';
  private router_: ComposeDialogCallbackRouter =
      new ComposeDialogCallbackRouter();
  remote = this.router_.$.bindNewPipeAndPassRemote();

  constructor() {
    super(['compose']);
  }

  compose(style: StyleModifiers, input: string): void {
    this.remote.responseReceived(
        {status: ComposeStatus.kOk, result: this.composeOutput_});
    this.methodCalled('compose', {style, input});
  }

  /** @override */
  getRouter() {
    return this.router_;
  }

  setComposeOutput(output: string) {
    this.composeOutput_ = output;
  }
}

suite('ComposeApp', () => {
  let app: ComposeAppElement;
  let testProxy: TestingApiProxy;

  setup(() => {
    testProxy = new TestingApiProxy();
    ComposeApiProxyImpl.setInstance(testProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    app = document.createElement('compose-app');
    document.body.appendChild(app);

    return flushTasks();
  });

  function mockInput(input: string) {
    app.$.textarea.value = input;
    app.$.textarea.dispatchEvent(new CustomEvent('value-changed'));
  }
  test('SubmitsInput', async () => {
    // Starts off with submit disabled since input is empty.
    assertTrue(isVisible(app.$.submitButton));
    assertTrue(app.$.submitButton.disabled);
    assertFalse(isVisible(app.$.resultContainer));
    assertFalse(isVisible(app.$.insertButton));

    // Invalid input keeps submit disabled.
    mockInput('Short');
    assertTrue(app.$.submitButton.disabled);

    // Inputting valid text enables submit.
    mockInput('Here is my input.');
    assertFalse(app.$.submitButton.disabled);

    // Clicking on submit gets results.
    app.$.submitButton.click();
    assertTrue(isVisible(app.$.loading));

    const args = await testProxy.whenCalled('compose');
    await testProxy.remote.$.flushForTesting();

    assertEquals(Length.kUnset, args.style.length);
    assertEquals(Tone.kUnset, args.style.tone);
    assertEquals('Here is my input.', args.input);

    assertFalse(isVisible(app.$.loading));
    assertFalse(isVisible(app.$.submitButton));
    assertTrue(app.$.textarea.readonly);
    assertTrue(isVisible(app.$.insertButton));
  });

  test('RefreshesResult', async () => {
    // Submit the input once so the refresh button shows up.
    testProxy.setComposeOutput('Outdated output.');
    mockInput('Input to refresh.');
    app.$.submitButton.click();
    await testProxy.remote.$.flushForTesting();
    testProxy.resetResolver('compose');
    assertTrue(
        isVisible(app.$.refreshButton), 'Refresh button should be visible.');

    // Click the refresh button and assert compose is called with the same args.
    testProxy.setComposeOutput('Refreshed output.');
    app.$.refreshButton.click();
    assertTrue(
        isVisible(app.$.loading), 'Loading indicator should be visible.');

    const args = await testProxy.whenCalled('compose');
    await testProxy.remote.$.flushForTesting();

    assertEquals(Length.kUnset, args.style.length);
    assertEquals(Tone.kUnset, args.style.tone);
    assertEquals('Input to refresh.', args.input);

    // Verify UI has updated with refreshed results.
    assertFalse(isVisible(app.$.loading));
    assertTrue(
        isVisible(app.$.resultContainer),
        'App result container should be visible.');
    assertTrue(
        app.$.resultContainer.textContent!.includes('Refreshed output.'));
  });

  test('UpdatesScrollableBodyAfterResults', async () => {
    assertTrue(app.$.body.hasAttribute('scrollable'));

    mockInput('Some fake input.');
    app.$.submitButton.click();

    // Mock a height on results to get body to scroll. The body should not yet
    // be scrollable though because result has not been fetched yet.
    app.$.resultContainer.style.minHeight = '500px';
    assertFalse(app.$.body.classList.contains('can-scroll'));

    await testProxy.whenCalled('compose');
    await whenCheck(
        app.$.body, () => app.$.body.classList.contains('can-scroll'));
    assertEquals(220, app.$.body.offsetHeight);
    assertTrue(220 < app.$.body.scrollHeight);
  });
});
