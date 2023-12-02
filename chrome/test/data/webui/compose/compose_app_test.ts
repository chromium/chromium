// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://compose/app.js';

import {ComposeAppElement, ComposeAppState} from 'chrome://compose/app.js';
import {CloseReason, ComposeDialogCallbackRouter, ComposeState, ComposeStatus, ConsentState, Length, OpenMetadata, StyleModifiers, Tone, UserFeedback} from 'chrome://compose/compose.mojom-webui.js';
import {ComposeApiProxy, ComposeApiProxyImpl} from 'chrome://compose/compose_api_proxy.js';
import {CrFeedbackOption} from 'chrome://resources/cr_elements/cr_feedback_buttons/cr_feedback_buttons.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {isVisible, whenCheck} from 'chrome://webui-test/test_util.js';

class TestingApiProxy extends TestBrowserProxy implements ComposeApiProxy {
  private initialConsentState_: ConsentState = ConsentState.kConsented;
  private initialInput_: string = '';
  private initialState_: ComposeState = {
    webuiState: '',
    feedback: UserFeedback.kUserFeedbackUnspecified,
    hasPendingRequest: false,
  };
  private router_: ComposeDialogCallbackRouter =
      new ComposeDialogCallbackRouter();
  remote = this.router_.$.bindNewPipeAndPassRemote();
  private undoResponse_: ComposeState|null = null;

  constructor() {
    super([
      'acceptComposeResult',
      'closeUi',
      'compose',
      'rewrite',
      'openBugReportingLink',
      'openFeedbackSurveyLink',
      'requestInitialState',
      'saveWebuiState',
      'setUserFeedback',
      'undo',
    ]);
  }

  acceptComposeResult(): Promise<boolean> {
    this.methodCalled('acceptComposeResult');
    return Promise.resolve(true);
  }

  acknowledgeConsentDisclaimer() {}

  approveConsent() {}

  closeUi(reason: CloseReason) {
    this.methodCalled('closeUi', reason);
  }

  compose(input: string, edited: boolean): void {
    this.methodCalled('compose', {input, edited});
  }

  rewrite(style: StyleModifiers): void {
    this.methodCalled('rewrite', {style});
  }

  undo(): Promise<(ComposeState | null)> {
    this.methodCalled('undo');
    return Promise.resolve(this.undoResponse_);
  }

  getRouter() {
    return this.router_;
  }

  openBugReportingLink() {
    this.methodCalled('openBugReportingLink');
  }

  openFeedbackSurveyLink() {
    this.methodCalled('openFeedbackSurveyLink');
  }

  openComposeSettings() {}

  requestInitialState(): Promise<OpenMetadata> {
    this.methodCalled('requestInitialState');
    return Promise.resolve({
      consentState: this.initialConsentState_,
      composeState: this.initialState_,
      initialInput: this.initialInput_,
      configurableParams: {
        minWordLimit: 2,
        maxWordLimit: 50,
        maxCharacterLimit: 100,
      },
    });
  }

  saveWebuiState(state: string) {
    this.methodCalled('saveWebuiState', state);
  }

  setUserFeedback(feedback: UserFeedback) {
    this.methodCalled('setUserFeedback', feedback);
  }

  setInitialConsentState(consent: ConsentState) {
    this.initialConsentState_ = consent;
  }

  setInitialState(state: Partial<ComposeState>, input?: string) {
    this.initialState_ = Object.assign(
        {
          webuiState: '',
          style: {tone: Tone.kUnset, length: Length.kUnset},
          feedback: UserFeedback.kUserFeedbackUnspecified,
          hasPendingRequest: false,
        },
        state);
    this.initialInput_ = input || '';
  }

  setUndoResponse(state: ComposeState|null) {
    this.undoResponse_ = state;
  }

  showUi() {}
}

suite('ComposeApp', () => {
  let app: ComposeAppElement;
  let testProxy: TestingApiProxy;

  setup(async () => {
    testProxy = new TestingApiProxy();
    ComposeApiProxyImpl.setInstance(testProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    app = document.createElement('compose-app');
    document.body.appendChild(app);

    await testProxy.whenCalled('requestInitialState');
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

  async function initializeNewAppWithConsentState(consent: ConsentState):
      Promise<ComposeAppElement> {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testProxy.setInitialConsentState(consent);
    const newApp = document.createElement('compose-app');
    document.body.appendChild(newApp);
    await flushTasks();
    return newApp;
  }

  test('SendsInputParams', () => {
    assertEquals(2, app.$.textarea.inputParams.minWordLimit);
    assertEquals(50, app.$.textarea.inputParams.maxWordLimit);
    assertEquals(100, app.$.textarea.inputParams.maxCharacterLimit);
  });

  test('SubmitsAndAcceptsInput', async () => {
    // Starts off with submit enabled even when input is empty.
    assertTrue(isVisible(app.$.submitButton));
    assertFalse(app.$.submitButton.disabled);
    assertFalse(isVisible(app.$.resultContainer));
    assertFalse(isVisible(app.$.insertButton));

    // Invalid input keeps submit enabled and error is not visible.
    mockInput('Short');
    assertFalse(app.$.submitButton.disabled);
    assertFalse(isVisible(app.$.textarea.$.tooShortError));
    assertFalse(isVisible(app.$.textarea.$.tooLongError));

    // Clicking on submit shows error.
    app.$.submitButton.click();
    assertTrue(app.$.submitButton.disabled);
    assertTrue(isVisible(app.$.textarea.$.tooShortError));
    assertFalse(isVisible(app.$.textarea.$.tooLongError));

    // Inputting valid text enables submit.
    mockInput('Here is my input.');
    assertFalse(app.$.submitButton.disabled);

    // Clicking on submit gets results.
    app.$.submitButton.click();
    assertTrue(isVisible(app.$.loading));

    const args = await testProxy.whenCalled('compose');
    await mockResponse();

    assertEquals('Here is my input.', args.input);

    assertFalse(isVisible(app.$.loading));
    assertFalse(isVisible(app.$.submitButton));
    assertTrue(app.$.textarea.readonly);
    assertTrue(isVisible(app.$.insertButton));

    // Clicking on Insert calls acceptComposeResult.
    app.$.insertButton.click();
    await testProxy.whenCalled('acceptComposeResult');
  });

  test('RefreshesResult', async () => {
    // Submit the input once so the refresh button shows up.
    mockInput('Input to refresh.');
    app.$.submitButton.click();
    await mockResponse();

    testProxy.resetResolver('rewrite');
    assertTrue(
        isVisible(app.$.refreshButton), 'Refresh button should be visible.');

    // Click the refresh button and assert compose is called with the same args.
    app.$.refreshButton.click();
    assertTrue(
        isVisible(app.$.loading), 'Loading indicator should be visible.');

    const args = await testProxy.whenCalled('rewrite');
    await mockResponse('Refreshed output.');

    // When the style length and tone are undefined, the request is to simply
    // rewrite the last response as-is.
    assertEquals(undefined, args.style.length);
    assertEquals(undefined, args.style.tone);

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
    await mockResponse();
    await whenCheck(
        app.$.body, () => app.$.body.classList.contains('can-scroll'));
    assertEquals(220, app.$.body.offsetHeight);
    assertTrue(220 < app.$.body.scrollHeight);
  });

  test('ConsentStateDeterminesViewState', async () => {
    const appWithConsentDialog =
        await initializeNewAppWithConsentState(ConsentState.kUnset);
    // Check correct visibility for consent view state
    assertFalse(isVisible(appWithConsentDialog.$.appDialog));
    assertTrue(isVisible(appWithConsentDialog.$.consentDialog));
    assertTrue(isVisible(appWithConsentDialog.$.consentFooter));
    assertFalse(isVisible(appWithConsentDialog.$.disclaimerFooter));

    const appWithDisclaimerDialog =
        await initializeNewAppWithConsentState(ConsentState.kExternalConsented);
    // Check correct visibility for disclaimer view state
    assertFalse(isVisible(appWithDisclaimerDialog.$.appDialog));
    assertTrue(isVisible(appWithDisclaimerDialog.$.consentDialog));
    assertFalse(isVisible(appWithDisclaimerDialog.$.consentFooter));
    assertTrue(isVisible(appWithDisclaimerDialog.$.disclaimerFooter));

    const appWithMainDialog =
        await initializeNewAppWithConsentState(ConsentState.kConsented);
    // Check correct visibility for main app view state
    assertTrue(isVisible(appWithMainDialog.$.appDialog));
    assertFalse(isVisible(appWithMainDialog.$.consentDialog));
  });

  test('ConsentCloseButton', async () => {
    const appWithConsentDialog =
        await initializeNewAppWithConsentState(ConsentState.kUnset);

    appWithConsentDialog.$.closeButtonConsent.click();
    // Close reason should match that given to the consent close button.
    const closeReason = await testProxy.whenCalled('closeUi');
    assertEquals(CloseReason.kConsentCloseButton, closeReason);
  });

  test('ConsentNoThanksButton', async () => {
    const appWithConsentDialog =
        await initializeNewAppWithConsentState(ConsentState.kUnset);

    appWithConsentDialog.$.consentNoThanksButton.click();
    // Close reason should match that given to the consent no thanks button.
    const closeReason = await testProxy.whenCalled('closeUi');
    assertEquals(CloseReason.kPageContentConsentDeclined, closeReason);
  });

  test('ConsentYesButton', async () => {
    const appWithConsentDialog =
        await initializeNewAppWithConsentState(ConsentState.kUnset);

    appWithConsentDialog.$.consentYesButton.click();
    // View state should change from consent UI to main app UI.
    assertFalse(isVisible(appWithConsentDialog.$.consentDialog));
    assertTrue(isVisible(appWithConsentDialog.$.appDialog));
  });

  test('DisclaimerLetsGoButton', async () => {
    const appWithDisclaimerDialog =
        await initializeNewAppWithConsentState(ConsentState.kExternalConsented);

    appWithDisclaimerDialog.$.disclaimerLetsGoButton.click();
    // View state should change from disclaimer UI to main app UI.
    assertFalse(isVisible(appWithDisclaimerDialog.$.consentDialog));
    assertTrue(isVisible(appWithDisclaimerDialog.$.appDialog));
  });

  test('InitializesWithState', async () => {
    async function initializeNewAppWithState(
        state: Partial<ComposeState>,
        input?: string): Promise<ComposeAppElement> {
      document.body.innerHTML = window.trustedTypes!.emptyHTML;
      testProxy.setInitialState(state, input);
      const newApp = document.createElement('compose-app');
      document.body.appendChild(newApp);
      await flushTasks();
      return newApp;
    }

    // Initial input
    const appWithInitialInput =
        await initializeNewAppWithState({}, 'initial input');
    assertEquals('initial input', appWithInitialInput.$.textarea.value);

    // Invalid input is sent to textarea but submit is enabled.
    const appWithInvalidInput = await initializeNewAppWithState(
        {webuiState: JSON.stringify({input: 'short'})});
    assertEquals('short', appWithInvalidInput.$.textarea.value);
    assertFalse(appWithInvalidInput.$.submitButton.disabled);

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
    assertTrue(appWithResult.$.undoButton.disabled);

    // Input with response with undo available.
    const appWithUndo = await initializeNewAppWithState({
      webuiState: JSON.stringify({input: 'some input'}),
      hasPendingRequest: false,
      response: {
        status: ComposeStatus.kOk,
        undoAvailable: true,
        result: 'here is a result',
      },
    });
    assertFalse(appWithUndo.$.undoButton.disabled);

    // Already has a response but is loading another one.
    const appWithResultAndLoading = await initializeNewAppWithState({
      webuiState: JSON.stringify({input: 'some input'}),
      hasPendingRequest: true,
      response: {
        status: ComposeStatus.kOk,
        undoAvailable: false,
        result: 'here is a result',
      },
    });
    assertTrue(isVisible(appWithResultAndLoading.$.loading));
    assertFalse(isVisible(appWithResultAndLoading.$.resultContainer));

    // Input with response while editing input shows edit textarea.
    const appEditingPrompt = await initializeNewAppWithState({
      webuiState: JSON.stringify({
        input: 'some input',
        isEditingSubmittedInput: true,
        editedInput: 'some new input',
      }),
      hasPendingRequest: false,
      response: {
        status: ComposeStatus.kOk,
        undoAvailable: false,
        result: 'here is a result',
      },
    });
    assertTrue(isVisible(appEditingPrompt.$.editTextarea));
    assertEquals('some new input', appEditingPrompt.$.editTextarea.value);
    assertEquals(
        'hidden',
        window.getComputedStyle(appEditingPrompt.$.textarea).visibility);
    assertFalse(isVisible(appEditingPrompt.$.loading));
    assertEquals(
        'hidden',
        window.getComputedStyle(appEditingPrompt.$.resultContainer).visibility);

    // Input with feedback already filled out.
    const appWithFeedback = await initializeNewAppWithState({
      feedback: UserFeedback.kUserFeedbackPositive,
    });
    const feedbackButtons =
        appWithFeedback.shadowRoot!.querySelector('cr-feedback-buttons')!;
    assertEquals('true', feedbackButtons.$.thumbsUp.ariaPressed);
  });

  test('SavesState', async () => {
    assertEquals(0, testProxy.getCallCount('saveWebuiState'), 'es');

    async function assertSavedState(expectedState: ComposeAppState) {
      const savedState = await testProxy.whenCalled('saveWebuiState');
      assertDeepEquals(expectedState, JSON.parse(savedState));
      testProxy.resetResolver('saveWebuiState');
    }

    // Changing input saves state.
    mockInput('Here is my input');
    await assertSavedState({input: 'Here is my input'});

    // Visibilitychange event saves state.
    Object.defineProperty(
        document, 'visibilityState', {value: 'hidden', writable: true});
    document.dispatchEvent(new CustomEvent('visibilitychange'));
    await assertSavedState({input: 'Here is my input'});

    // Hitting submit saves state.
    app.$.submitButton.click();
    await assertSavedState({input: 'Here is my input'});

    // Hitting edit button saves state.
    app.$.textarea.dispatchEvent(
        new CustomEvent('edit-click', {composed: true, bubbles: true}));
    await assertSavedState({
      editedInput: 'Here is my input',
      input: 'Here is my input',
      isEditingSubmittedInput: true,
    });

    // Updating edit textarea saves state.
    app.$.editTextarea.value = 'Here is my new input';
    app.$.editTextarea.dispatchEvent(new CustomEvent('value-changed'));
    await assertSavedState({
      editedInput: 'Here is my new input',
      input: 'Here is my input',
      isEditingSubmittedInput: true,
    });

    // Canceling reverts state back to before editing.
    app.$.cancelEditButton.click();
    await assertSavedState({input: 'Here is my input'});

    // Submitting edited textarea saves state.
    app.$.textarea.dispatchEvent(
        new CustomEvent('edit-click', {composed: true, bubbles: true}));
    app.$.editTextarea.value = 'Here is my new input!!!!';
    app.$.editTextarea.dispatchEvent(new CustomEvent('value-changed'));
    testProxy.resetResolver('saveWebuiState');
    app.$.submitEditButton.click();
    await assertSavedState({input: 'Here is my new input!!!!'});
  });

  test('DebouncesSavingState', async () => {
    mockInput('Here is my input');
    mockInput('Here is my input 2');
    await flushTasks();
    const savedState = await testProxy.whenCalled('saveWebuiState');
    assertEquals(1, testProxy.getCallCount('saveWebuiState'));
    assertEquals(JSON.stringify({input: 'Here is my input 2'}), savedState);
  });

  test('DebouncesSavingState', async () => {
    mockInput('Here is my input');
    mockInput('Here is my input 2');
    await flushTasks();
    const savedState = await testProxy.whenCalled('saveWebuiState');
    assertEquals(1, testProxy.getCallCount('saveWebuiState'));
    assertEquals(JSON.stringify({input: 'Here is my input 2'}), savedState);
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

  test('AllowsEditingPrompt', async () => {
    app.$.textarea.dispatchEvent(
        new CustomEvent('edit-click', {composed: true, bubbles: true}));
    assertTrue(isVisible(app.$.editTextarea));

    mockInput('Initial input.');
    app.$.submitButton.click();
    await testProxy.whenCalled('compose');
    await flushTasks();
    testProxy.resetResolver('compose');

    // Mock changing length and tone to verify they are unset after editing
    // the input.
    app.$.lengthMenu.value = `${Length.kShorter}`;
    app.$.lengthMenu.dispatchEvent(new CustomEvent('change'));
    app.$.toneMenu.value = `${Tone.kCasual}`;
    app.$.toneMenu.dispatchEvent(new CustomEvent('change'));
    await flushTasks();
    testProxy.resetResolver('compose');

    // Mock clicking edit in the textarea and verify new textarea shows.
    app.$.textarea.dispatchEvent(
        new CustomEvent('edit-click', {composed: true, bubbles: true}));
    assertTrue(isVisible(app.$.editTextarea));

    // Mock updating input and cancelling.
    assertEquals('Initial input.', app.$.editTextarea.value);
    app.$.editTextarea.value = 'Here is a better input.';
    app.$.editTextarea.dispatchEvent(new CustomEvent('value-changed'));
    app.$.cancelEditButton.click();
    assertFalse(isVisible(app.$.editTextarea));
    assertEquals('Initial input.', app.$.textarea.value);

    // Mock updating input and submitting.
    app.$.textarea.dispatchEvent(
        new CustomEvent('edit-click', {composed: true, bubbles: true}));
    app.$.editTextarea.value = 'Here is an even better input.';
    app.$.editTextarea.dispatchEvent(new CustomEvent('value-changed'));
    app.$.submitEditButton.click();
    assertFalse(isVisible(app.$.editTextarea));
    assertEquals('Here is an even better input.', app.$.textarea.value);

    const args = await testProxy.whenCalled('compose');
    await mockResponse('new response');
    assertEquals('Here is an even better input.', args.input);
    assertTrue(args.edited);
    assertTrue(app.$.resultContainer.textContent!.includes('new response'));
  });

  test('ComposeWithLengthToneOptionResult', async () => {
    // Submit the input once so the refresh button shows up.
    mockInput('Input to refresh.');
    app.$.submitButton.click();
    await mockResponse();

    testProxy.resetResolver('rewrite');

    assertTrue(isVisible(app.$.lengthMenu), 'Length menu should be visible.');
    assertEquals(
        2, app.$.lengthMenu.querySelectorAll('option:not([hidden])').length);

    app.$.lengthMenu.value = `${Length.kShorter}`;
    app.$.lengthMenu.dispatchEvent(new CustomEvent('change'));

    const args = await testProxy.whenCalled('rewrite');
    await mockResponse();

    assertEquals(Length.kShorter, args.style.length);

    testProxy.resetResolver('rewrite');

    assertTrue(isVisible(app.$.toneMenu), 'Tone menu should be visible.');
    assertEquals(
        2, app.$.toneMenu.querySelectorAll('option:not([hidden])').length);

    app.$.toneMenu.value = `${Tone.kCasual}`;
    app.$.toneMenu.dispatchEvent(new CustomEvent('change'));

    const args2 = await testProxy.whenCalled('rewrite');
    await mockResponse();

    assertEquals(Tone.kCasual, args2.style.tone);
  });

  test('Undo', async () => {
    // Set up initial state to show undo button and mock up a previous state.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testProxy.setInitialState({
      hasPendingRequest: false,
      response: {
        status: ComposeStatus.kOk,
        undoAvailable: true,
        result: 'here is a result',
      },
    });
    testProxy.setUndoResponse({
      hasPendingRequest: false,
      response: {
        status: ComposeStatus.kOk,
        undoAvailable: false,
        result: 'some undone result',
      },
      webuiState: JSON.stringify({
        input: 'my old input',
        selectedLength: Number(Length.kLonger),
        selectedTone: Number(Tone.kCasual),
      }),
      feedback: UserFeedback.kUserFeedbackUnspecified,
    });
    const appWithUndo = document.createElement('compose-app');
    document.body.appendChild(appWithUndo);
    await testProxy.whenCalled('requestInitialState');

    // CLick undo.
    appWithUndo.$.undoButton.click();
    await testProxy.whenCalled('undo');

    // UI is updated.
    assertEquals('my old input', appWithUndo.$.textarea.value);
    assertTrue(isVisible(appWithUndo.$.resultContainer));
    assertTrue(appWithUndo.$.resultContainer.textContent!.includes(
        'some undone result'));
    assertEquals(Length.kLonger, Number(appWithUndo.$.lengthMenu.value));
    assertEquals(Tone.kCasual, Number(appWithUndo.$.toneMenu.value));
  });

  test('Feedback', async () => {
    const feedbackButtons =
        app.shadowRoot!.querySelector('cr-feedback-buttons')!;
    feedbackButtons.dispatchEvent(new CustomEvent('selected-option-changed', {
      bubbles: true,
      composed: true,
      detail: {value: CrFeedbackOption.THUMBS_DOWN},
    }));
    const args = await testProxy.whenCalled('setUserFeedback');
    assertEquals(args.reason, args.UserFeedback);
  });
});
