// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://compose/app.js';

import {ComposeAppElement} from 'chrome://compose/app.js';
import {CloseReason, ComposeDialogCallbackRouter, ComposeState, ComposeStatus, Length, OpenMetadata, StyleModifiers, Tone} from 'chrome://compose/compose.mojom-webui.js';
import {ComposeApiProxy, ComposeApiProxyImpl} from 'chrome://compose/compose_api_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {isVisible, whenCheck} from 'chrome://webui-test/test_util.js';


class TestingApiProxy extends TestBrowserProxy implements ComposeApiProxy {
  private initialState_: ComposeState = {
    webuiState: '',
    style: {tone: Tone.kUnset, length: Length.kUnset},
    hasPendingRequest: false,
  };
  private router_: ComposeDialogCallbackRouter =
      new ComposeDialogCallbackRouter();
  remote = this.router_.$.bindNewPipeAndPassRemote();

  constructor() {
    super(['closeUi', 'compose', 'requestInitialState', 'saveWebuiState']);
  }

  closeUi(reason: CloseReason) {
    this.methodCalled('closeUi', reason);
  }

  compose(style: StyleModifiers, input: string): void {
    this.methodCalled('compose', {style, input});
  }

  undo(): Promise<(ComposeState | null)> {
    return Promise.resolve(null);
  }

  getRouter() {
    return this.router_;
  }

  requestInitialState(): Promise<OpenMetadata> {
    this.methodCalled('requestInitialState');
    return Promise.resolve({
      composeState: this.initialState_,
      initialInput: '',
    });
  }

  saveWebuiState(state: string) {
    this.methodCalled('saveWebuiState', state);
  }

  setInitialState(state: Partial<ComposeState>) {
    this.initialState_ = Object.assign(
        {
          webuiState: '',
          style: {tone: Tone.kUnset, length: Length.kUnset},
          hasPendingRequest: false,
        },
        state);
  }

  acceptComposeResult() {}
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

  function mockResponse(
      result: string = 'some response',
      status: ComposeStatus = ComposeStatus.kOk): Promise<void> {
    testProxy.remote.responseReceived(
        {status: status, undoAvailable: false, result});
    return testProxy.remote.$.flushForTesting();
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
    await mockResponse();

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
    mockInput('Input to refresh.');
    app.$.submitButton.click();
    await mockResponse();

    testProxy.resetResolver('compose');
    assertTrue(
        isVisible(app.$.refreshButton), 'Refresh button should be visible.');

    // Click the refresh button and assert compose is called with the same args.
    app.$.refreshButton.click();
    assertTrue(
        isVisible(app.$.loading), 'Loading indicator should be visible.');

    const args = await testProxy.whenCalled('compose');
    await mockResponse('Refreshed output.');

    assertEquals(Length.kUnset, args.style.length);
    assertEquals(Tone.kUnset, args.style.tone);
    assertEquals('Input to refresh.', args.input);

    // // Verify UI has updated with refreshed results.
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
    await mockResponse();
    await whenCheck(
        app.$.body, () => app.$.body.classList.contains('can-scroll'));
    assertEquals(220, app.$.body.offsetHeight);
    assertTrue(220 < app.$.body.scrollHeight);
  });

  test('InitializesWithState', async () => {
    async function initializeNewAppWithState(state: Partial<ComposeState>):
        Promise<ComposeAppElement> {
      document.body.innerHTML = window.trustedTypes!.emptyHTML;
      testProxy.setInitialState(state);
      const newApp = document.createElement('compose-app');
      document.body.appendChild(newApp);
      await flushTasks();
      return newApp;
    }

    // Invalid input is sent to textarea but submit is still disabled.
    const appWithInvalidInput = await initializeNewAppWithState(
        {webuiState: JSON.stringify({input: 'short'})});
    assertEquals('short', appWithInvalidInput.$.textarea.value);
    assertTrue(appWithInvalidInput.$.submitButton.disabled);

    // Valid input is sent to textarea and submit is enabled.
    const appWithValidInput = await initializeNewAppWithState(
        {webuiState: JSON.stringify({input: 'not short at all'})});
    assertEquals('not short at all', appWithValidInput.$.textarea.value);
    assertFalse(appWithValidInput.$.submitButton.disabled);

    // Input with pending response shows loading state.
    const appWithLoadingState = await initializeNewAppWithState({
      hasPendingRequest: true,
      webuiState: JSON.stringify({input: 'some input'}),
    });
    assertTrue(isVisible(appWithLoadingState.$.loading));

    // Input with response shows response.
    const appWithResult = await initializeNewAppWithState({
      webuiState: JSON.stringify({input: 'some input'}),
      hasPendingRequest: false,
      response: {
        status: ComposeStatus.kOk,
        undoAvailable: false,
        result: 'here is a result',
      },
    });
    assertTrue(isVisible(appWithResult.$.resultContainer));
    assertTrue(appWithResult.$.resultContainer.textContent!.includes(
        'here is a result'));
  });

  test('SavesState', async () => {
    assertEquals(0, testProxy.getCallCount('saveWebuiState'));

    // Changing input saves state.
    mockInput('Here is my input');
    let savedState = await testProxy.whenCalled('saveWebuiState');
    assertEquals(JSON.stringify({input: 'Here is my input'}), savedState);
    testProxy.resetResolver('saveWebuiState');

    // Hitting submit saves state.
    app.$.submitButton.click();
    savedState = await testProxy.whenCalled('saveWebuiState');
    assertEquals(JSON.stringify({input: 'Here is my input'}), savedState);
  });

  test('CloseButton', async () => {
    assertTrue(isVisible(app.$.closeButton));

    app.$.closeButton.click();
    // Close reason should match that given to the close button.
    const closeReason = await testProxy.whenCalled('closeUi');
    assertEquals(CloseReason.kCloseButton, closeReason);
  });

  test('ErrorFooterShowsMessage', async () => {
    async function testError(status: ComposeStatus, stringKey: string) {
      const errorMessage = `some error ${stringKey}`;
      loadTimeData.overrideValues({[stringKey]: errorMessage});

      mockInput('Here is my input.');
      app.$.submitButton.click();
      await testProxy.whenCalled('compose');
      await mockResponse('', status);

      assertTrue(isVisible(app.$.errorFooter));
      assertTrue(app.$.errorFooter.textContent!.includes(errorMessage));
    }

    testError(ComposeStatus.kError, 'errorGeneric');
    testError(ComposeStatus.kNotSuccessful, 'errorRequestNotSuccessful');
    testError(ComposeStatus.kTryAgainLater, 'errorTryAgainLater');
    testError(ComposeStatus.kTryAgain, 'errorTryAgain');
    testError(ComposeStatus.kPermissionDenied, 'errorPermissionDenied');
    testError(ComposeStatus.kMisconfiguration, 'errorGeneric');
  });
});
