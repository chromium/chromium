// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://compose/app.js';

import {CrFeedbackOption} from '//resources/cr_elements/cr_feedback_buttons/cr_feedback_buttons.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {ComposeAppElement, ComposeAppState} from 'chrome-untrusted://compose/app.js';
import type {ComposeState} from 'chrome-untrusted://compose/compose.mojom-webui.js';
import {CloseReason, InputMode, StyleModifier, UserFeedback} from 'chrome-untrusted://compose/compose.mojom-webui.js';
import {ComposeApiProxyImpl} from 'chrome-untrusted://compose/compose_api_proxy.js';
import {ComposeStatus} from 'chrome-untrusted://compose/compose_enums.mojom-webui.js';
import {assertDeepEquals, assertEquals, assertFalse, assertStringContains, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {flushTasks} from 'chrome-untrusted://webui-test/polymer_test_util.js';
import {isVisible, whenCheck} from 'chrome-untrusted://webui-test/test_util.js';

import {TestComposeApiProxy} from './test_compose_api_proxy.js';

suite('ComposeApp', () => {
  let app: ComposeAppElement;
  let testProxy: TestComposeApiProxy;

  setup(async () => {
    testProxy = new TestComposeApiProxy();
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
      status: ComposeStatus = ComposeStatus.kOk, onDeviceEvaluationUsed = false,
      triggeredFromModifier = false): Promise<void> {
    testProxy.remote.responseReceived({
      status: status,
      undoAvailable: false,
      redoAvailable: false,
      providedByUser: false,
      result,
      onDeviceEvaluationUsed,
      triggeredFromModifier,
    });
    return testProxy.remote.$.flushForTesting();
  }

  function mockPartialResponse(result: string = 'partial response'):
      Promise<void> {
    testProxy.remote.partialResponseReceived({result});
    return testProxy.remote.$.flushForTesting();
  }

  async function initializeNewAppWithFirstRunAndMsbbState(
      fre: boolean, msbb: boolean): Promise<ComposeAppElement> {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testProxy.setOpenMetadata({freComplete: fre, msbbState: msbb});
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
    assertFalse(isVisible(app.$.acceptButton));

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
    assertTrue(isVisible(app.$.acceptButton));
    assertFalse(isVisible(app.$.onDeviceUsedFooter));

    // Clicking on accept button calls acceptComposeResult.
    app.$.acceptButton.click();
    await testProxy.whenCalled('acceptComposeResult');
  });

  test('OnlyOneErrorShows', async () => {
    mockInput('x'.repeat(2501));
    app.$.submitButton.click();
    assertTrue(app.$.submitButton.disabled);
    assertTrue(isVisible(app.$.textarea.$.tooLongError));
    assertFalse(isVisible(app.$.textarea.$.tooShortError));
  });

  test('AcceptButtonText', async () => {
    async function initializeNewAppWithTextSelectedState(textSelected: boolean):
        Promise<ComposeAppElement> {
      document.body.innerHTML = window.trustedTypes!.emptyHTML;
      testProxy.setOpenMetadata({textSelected});
      const newApp = document.createElement('compose-app');
      document.body.appendChild(newApp);
      await flushTasks();
      return newApp;
    }
    const appWithTextSelected =
        await initializeNewAppWithTextSelectedState(true);
    assertStringContains(
        appWithTextSelected.$.acceptButton.textContent!, 'Replace');

    const appWithNoTextSelected =
        await initializeNewAppWithTextSelectedState(false);
    assertStringContains(
        appWithNoTextSelected.$.acceptButton.textContent!, 'Insert');
  });

  test('FirstRunAndMsbbStateDetermineViewState', async () => {
    // Check correct visibility for FRE view state.
    const appWithFirstRunDialog =
        await initializeNewAppWithFirstRunAndMsbbState(false, false);
    assertTrue(isVisible(appWithFirstRunDialog.$.firstRunDialog));
    assertFalse(isVisible(appWithFirstRunDialog.$.freMsbbDialog));
    assertFalse(isVisible(appWithFirstRunDialog.$.appDialog));

    // Check correct visibility for MSBB view state.
    const appWithMSBBDialog =
        await initializeNewAppWithFirstRunAndMsbbState(true, false);
    assertFalse(isVisible(appWithMSBBDialog.$.firstRunDialog));
    assertTrue(isVisible(appWithMSBBDialog.$.freMsbbDialog));
    assertFalse(isVisible(appWithMSBBDialog.$.appDialog));

    // Check correct visibility for main app view state
    const appWithMainDialog =
        await initializeNewAppWithFirstRunAndMsbbState(true, true);
    assertFalse(isVisible(appWithMainDialog.$.firstRunDialog));
    assertFalse(isVisible(appWithMainDialog.$.freMsbbDialog));
    assertTrue(isVisible(appWithMainDialog.$.appDialog));
  });


  test('FirstRunCloseButton', async () => {
    const appWithFirstRunDialog =
        await initializeNewAppWithFirstRunAndMsbbState(true, false);

    appWithFirstRunDialog.$.firstRunCloseButton.click();
    // Close reason should match that given to the FRE close button.
    const closeReason = await testProxy.whenCalled('closeUi');
    assertEquals(CloseReason.kFirstRunCloseButton, closeReason);
  });

  test('MSBBCloseButton', async () => {
    const appWithMsbbDialog =
        await initializeNewAppWithFirstRunAndMsbbState(true, false);

    appWithMsbbDialog.$.closeButtonMSBB.click();
    // Close reason should match that given to the consent close button.
    const closeReason = await testProxy.whenCalled('closeUi');
    assertEquals(CloseReason.kMSBBCloseButton, closeReason);
  });

  test('FirstRunOkButtonToMainDialog', async () => {
    const appWithFirstRunDialog =
        await initializeNewAppWithFirstRunAndMsbbState(false, true);

    appWithFirstRunDialog.$.firstRunOkButton.click();
    // View state should change from FRE UI to main app UI.
    assertFalse(isVisible(appWithFirstRunDialog.$.firstRunDialog));
    assertFalse(isVisible(appWithFirstRunDialog.$.freMsbbDialog));
    assertTrue(isVisible(appWithFirstRunDialog.$.appDialog));
  });

  test('FirstRunOkButtonToMSBBDialog', async () => {
    const appWithFirstRunDialog =
        await initializeNewAppWithFirstRunAndMsbbState(false, false);

    appWithFirstRunDialog.$.firstRunOkButton.click();
    // View state should change from FRE UI to MSBB UI.
    assertFalse(isVisible(appWithFirstRunDialog.$.firstRunDialog));
    assertTrue(isVisible(appWithFirstRunDialog.$.freMsbbDialog));
    assertFalse(isVisible(appWithFirstRunDialog.$.appDialog));
  });

  test('InitializesWithState', async () => {
    async function initializeNewAppWithState(
        state: Partial<ComposeState>,
        input?: string): Promise<ComposeAppElement> {
      document.body.innerHTML = window.trustedTypes!.emptyHTML;
      testProxy.setOpenMetadata({initialInput: input}, state);
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
        redoAvailable: false,
        providedByUser: false,
        result: 'here is a result',
        onDeviceEvaluationUsed: false,
        triggeredFromModifier: false,
      },
    });
    assertTrue(isVisible(appWithResult.$.resultContainer));
    assertStringContains(
        appWithResult.$.resultText.$.root.innerText, 'here is a result');
    assertTrue(appWithResult.$.undoButton.disabled);

    // Input with response with undo available.
    const appWithUndo = await initializeNewAppWithState({
      webuiState: JSON.stringify({input: 'some input'}),
      hasPendingRequest: false,
      response: {
        status: ComposeStatus.kOk,
        undoAvailable: true,
        redoAvailable: false,
        providedByUser: false,
        result: 'here is a result',
        onDeviceEvaluationUsed: false,
        triggeredFromModifier: false,
      },
    });
    assertFalse(appWithUndo.$.undoButton.disabled);

    // Input with positive feedback.
    const appWithPositiveFeedback = await initializeNewAppWithState({
      webuiState: JSON.stringify({input: 'some input'}),
      hasPendingRequest: false,
      feedback: UserFeedback.kUserFeedbackPositive,
    });
    assertEquals(
        CrFeedbackOption.THUMBS_UP,
        appWithPositiveFeedback.$.feedbackButtons.selectedOption);

    // Already has a response but is loading another one.
    const appWithResultAndLoading = await initializeNewAppWithState({
      webuiState: JSON.stringify({input: 'some input'}),
      hasPendingRequest: true,
      response: {
        status: ComposeStatus.kOk,
        undoAvailable: false,
        redoAvailable: false,
        providedByUser: false,
        result: 'here is a result',
        onDeviceEvaluationUsed: false,
        triggeredFromModifier: false,
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
        redoAvailable: false,
        providedByUser: false,
        result: 'here is a result',
        onDeviceEvaluationUsed: false,
        triggeredFromModifier: false,
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

  test('InitializesWithInputModeStateAndUpdatesMode', async () => {
    async function initializeNewAppWithInputMode(mode: InputMode):
        Promise<ComposeAppElement> {
      document.body.innerHTML = window.trustedTypes!.emptyHTML;
      const compose_app_state: ComposeAppState = {
        input: '',
        inputMode: mode,
      };
      const state: Partial<ComposeState> = {
        webuiState: JSON.stringify(compose_app_state),
      };
      testProxy.setOpenMetadata({}, state);
      const newApp = document.createElement('compose-app');
      document.body.appendChild(newApp);
      await flushTasks();
      return newApp;
    }

    // Initial input
    const appWithInputMode =
        await initializeNewAppWithInputMode(InputMode.kElaborate);
    assertTrue(
        appWithInputMode.$.elaborateChip.selected,
        'Elaborate mode chip should be selected.');
    assertFalse(appWithInputMode.$.polishChip.selected);
    assertFalse(appWithInputMode.$.formalizeChip.selected);

    // Change the selected input mode
    appWithInputMode.$.formalizeChip.click();
    assertTrue(
        appWithInputMode.$.formalizeChip.selected,
        'Formalize mode chip should be selected.');
    assertFalse(appWithInputMode.$.polishChip.selected);
    assertFalse(appWithInputMode.$.elaborateChip.selected);
  });

  test('InputModesSaveState', async () => {
    assertEquals(0, testProxy.getCallCount('saveWebuiState'));

    async function assertSavedState(expectedState: ComposeAppState) {
      const savedState = await testProxy.whenCalled('saveWebuiState');
      assertDeepEquals(expectedState, JSON.parse(savedState));
      testProxy.resetResolver('saveWebuiState');
    }

    mockInput('Here is my input');
    // Changing the mode saves state.
    app.$.elaborateChip.click();
    await assertSavedState(
        {input: 'Here is my input', inputMode: InputMode.kElaborate});
    app.$.formalizeChip.click();
    await assertSavedState(
        {input: 'Here is my input', inputMode: InputMode.kFormalize});
    app.$.polishChip.click();
    await assertSavedState(
        {input: 'Here is my input', inputMode: InputMode.kPolish});
  });

  test('CloseButton', async () => {
    assertTrue(isVisible(app.$.closeButton));

    app.$.closeButton.click();
    // Close reason should match that given to the close button.
    const closeReason = await testProxy.whenCalled('closeUi');
    assertEquals(CloseReason.kCloseButton, closeReason);
  });

  test('GoBackFromError', async () => {
    testProxy.setResponseBeforeError({
      hasPendingRequest: false,
      response: {
        status: ComposeStatus.kOk,
        undoAvailable: false,
        redoAvailable: false,
        providedByUser: false,
        result: 'initial result text',
        onDeviceEvaluationUsed: false,
        triggeredFromModifier: false,
      },
      webuiState: JSON.stringify({
        input: 'initial input',
      }),
      feedback: UserFeedback.kUserFeedbackPositive,
    });

    // Mock a filtered error response that enables the go back button.
    mockInput('Initial input.');
    app.$.submitButton.click();
    const errorMessage = `filtered error message`;
    loadTimeData.overrideValues({['errorFiltered']: errorMessage});
    await mockResponse('', ComposeStatus.kFiltered, false, true);

    assertTrue(isVisible(app.$.errorFooter));
    assertStringContains(app.$.errorFooter.textContent!, errorMessage);
    assertTrue(isVisible(app.$.errorGoBackButton));

    app.$.errorGoBackButton.click();
    await testProxy.whenCalled('recoverFromErrorState');
    await flushTasks();

    // UI is updated to the mocked last ok response.
    assertEquals('initial input', app.$.textarea.value);
    assertTrue(isVisible(app.$.resultContainer));
    assertStringContains(
        app.$.resultText.$.root.innerText, 'initial result text');
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
      assertStringContains(app.$.errorFooter.textContent!, errorMessage);
    }

    await testError(ComposeStatus.kFiltered, 'errorFiltered');
    await testError(ComposeStatus.kRequestThrottled, 'errorRequestThrottled');
    await testError(ComposeStatus.kOffline, 'errorOffline');
    await testError(ComposeStatus.kRequestTimeout, 'errorTryAgainLater');
    await testError(ComposeStatus.kClientError, 'errorTryAgain');
    await testError(ComposeStatus.kMisconfiguration, 'errorTryAgain');
    await testError(ComposeStatus.kServerError, 'errorTryAgain');
    await testError(ComposeStatus.kInvalidRequest, 'errorTryAgain');
    await testError(ComposeStatus.kRetryableError, 'errorTryAgain');
    await testError(ComposeStatus.kNonRetryableError, 'errorTryAgain');
    await testError(ComposeStatus.kDisabled, 'errorTryAgain');
    await testError(ComposeStatus.kCancelled, 'errorTryAgain');
    await testError(ComposeStatus.kNoResponse, 'errorTryAgain');
  });

  test('UnsupportedLanguageErrorClickable', async () => {
    const errorMessage = `some error ${'errorUnsupportedLanguage'}`;
    loadTimeData.overrideValues({['errorUnsupportedLanguage']: errorMessage});

    mockInput('Here is my input.');
    app.$.submitButton.click();
    await testProxy.whenCalled('compose');
    await mockResponse('', ComposeStatus.kUnsupportedLanguage);

    assertTrue(isVisible(app.$.errorFooter));

    // Click on the "Learn more" link part of the error.
    (app.$.errorFooter.getElementsByTagName('A')[0] as HTMLElement).click();
    await testProxy.whenCalled('openComposeLearnMorePage');
  });

  test('PermissionDeniedErrorClickable', async () => {
    const errorMessage = `some error ${'errorPermissionDenied'}`;
    loadTimeData.overrideValues({['errorPermissionDenied']: errorMessage});

    mockInput('Here is my input.');
    app.$.submitButton.click();
    await testProxy.whenCalled('compose');
    await mockResponse('', ComposeStatus.kPermissionDenied);

    assertTrue(isVisible(app.$.errorFooter));

    // Click on the "Sign in" link part of the error.
    (app.$.errorFooter.getElementsByTagName('A')[1] as HTMLElement).click();
    await testProxy.whenCalled('openSignInPage');
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

    // Mock clicking edit in the textarea and verify new textarea shows.
    app.$.textarea.dispatchEvent(
        new CustomEvent('edit-click', {composed: true, bubbles: true}));
    await testProxy.whenCalled('logEditInput');
    assertTrue(isVisible(app.$.editTextarea));

    // Mock updating input and cancelling.
    assertEquals('Initial input.', app.$.editTextarea.value);
    app.$.editTextarea.value = 'Here is a better input.';
    app.$.editTextarea.dispatchEvent(new CustomEvent('value-changed'));
    app.$.cancelEditButton.click();
    await testProxy.whenCalled('logCancelEdit');
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
    assertStringContains(app.$.resultText.$.root.innerText, 'new response');
  });

  test('Undo', async () => {
    // Set up initial state to show undo button and mock up a previous state.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testProxy.setOpenMetadata({}, {
      hasPendingRequest: false,
      response: {
        status: ComposeStatus.kOk,
        undoAvailable: true,
        redoAvailable: false,
        providedByUser: false,
        result: 'here is a result',
        onDeviceEvaluationUsed: false,
        triggeredFromModifier: false,
      },
    });
    testProxy.setUndoResponse({
      hasPendingRequest: false,
      response: {
        status: ComposeStatus.kOk,
        undoAvailable: false,
        redoAvailable: false,
        providedByUser: false,
        result: 'some undone result',
        onDeviceEvaluationUsed: false,
        triggeredFromModifier: false,
      },
      webuiState: JSON.stringify({
        input: 'my old input',
      }),
      feedback: UserFeedback.kUserFeedbackPositive,
    });
    const appWithUndo = document.createElement('compose-app');
    document.body.appendChild(appWithUndo);
    await testProxy.whenCalled('requestInitialState');

    // Click undo.
    appWithUndo.$.undoButton.click();
    await testProxy.whenCalled('undo');
    await flushTasks();

    // UI is updated.
    assertEquals('my old input', appWithUndo.$.textarea.value);
    assertTrue(isVisible(appWithUndo.$.resultContainer));
    assertStringContains(
        appWithUndo.$.resultText.$.root.innerText, 'some undone result');
    assertEquals(
        CrFeedbackOption.THUMBS_UP,
        appWithUndo.$.feedbackButtons.selectedOption);
  });

  test('Redo', async () => {
    // Set up initial state to show redo button and mock a forward state to redo
    // to.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testProxy.setOpenMetadata({}, {
      hasPendingRequest: false,
      response: {
        status: ComposeStatus.kOk,
        undoAvailable: false,
        redoAvailable: true,
        providedByUser: false,
        result: 'here is a result',
        onDeviceEvaluationUsed: false,
        triggeredFromModifier: false,
      },
    });
    testProxy.setRedoResponse({
      hasPendingRequest: false,
      response: {
        status: ComposeStatus.kOk,
        undoAvailable: false,
        redoAvailable: false,
        providedByUser: false,
        result: 'some future result',
        onDeviceEvaluationUsed: false,
        triggeredFromModifier: false,
      },
      webuiState: JSON.stringify({
        input: 'some future input',
      }),
      feedback: UserFeedback.kUserFeedbackPositive,
    });
    const appWithRedo = document.createElement('compose-app');
    document.body.appendChild(appWithRedo);
    await testProxy.whenCalled('requestInitialState');

    // Click redo.
    appWithRedo.$.redoButton.click();
    await testProxy.whenCalled('redo');
    await flushTasks();

    // UI is updated.
    assertEquals('some future input', appWithRedo.$.textarea.value);
    assertTrue(isVisible(appWithRedo.$.resultContainer));
    assertStringContains(
        appWithRedo.$.resultText.$.root.innerText, 'some future result');
    assertEquals(
        CrFeedbackOption.THUMBS_UP,
        appWithRedo.$.feedbackButtons.selectedOption);
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

  test('PartialResponseIsShown', async () => {
    loadTimeData.overrideValues({
      enableOnDeviceDogfoodFooter: true,
    });

    // Make streaming work instantly.
    app.$.resultText.enableInstantStreamingForTesting();

    // Although streaming happens immediately, it requires a sequence of events.
    // Wait for those events to complete.
    const wait = async () => {
      for (let i = 0; i < 5; i++) {
        await flushTasks();
      }
    };

    mockInput('Some fake input.');
    app.$.submitButton.click();
    await testProxy.whenCalled('compose');

    // A partial response is shown.
    await mockPartialResponse('partial response here');
    await wait();
    assertTrue(
        isVisible(app.$.resultText.$.partialResultText),
        'partial result text should be shown');
    assertEquals(app.$.resultText.$.root.innerText, 'partial response');

    // The final response hides the partial response text.
    await mockResponse(
        'some response', ComposeStatus.kOk, /*onDeviceEvaluationUsed=*/
        true);
    await flushTasks();
    assertTrue(
        (app as any).showOnDeviceDogfoodFooter_(),
        'show footer should be true');
    await wait();
    assertTrue(
        isVisible(app.$.onDeviceUsedFooter),
        'on-device footer should be shown');
    assertEquals(app.$.resultText.$.root.innerText.trim(), 'some response');
  });

  test('RefreshesResult', async () => {
    // Submit the input once so that modifier menu is visible.
    mockInput('Input to retry.');
    app.$.submitButton.click();
    await mockResponse();

    testProxy.resetResolver('rewrite');
    assertTrue(
        isVisible(app.$.modifierMenu), 'Modifier menu should be visible.');

    // Select the retry option from the modifier menu and assert compose is
    // called with the same args.
    app.$.modifierMenu.value = `${StyleModifier.kRetry}`;
    app.$.modifierMenu.dispatchEvent(new CustomEvent('change'));
    assertTrue(
        isVisible(app.$.loading), 'Loading indicator should be visible.');

    const args = await testProxy.whenCalled('rewrite');
    await mockResponse('Refreshed output.');

    assertEquals(StyleModifier.kRetry, args);

    // Verify UI has updated with refreshed results.
    assertFalse(isVisible(app.$.loading));
    assertTrue(
        isVisible(app.$.resultContainer),
        'App result container should be visible.');
    assertStringContains(
        app.$.resultText.$.root.innerText, 'Refreshed output.');
  });

  test('UpdatesScrollableResultContainerAfterResize', async () => {
    // Assert scrolling container is set correctly.
    assertEquals(app.$.resultTextContainer, app.getContainer());
    mockInput('Some fake input.');
    app.$.submitButton.click();

    // The results text should not yet be visible because the result has not
    // been fetched yet.
    assertFalse(isVisible(app.$.resultTextContainer));

    // Results text should be scrollable when a long response is received.
    await testProxy.whenCalled('compose');
    const longResponse = 'x'.repeat(1000);
    await mockResponse(longResponse);
    await whenCheck(
        app.$.resultTextContainer,
        () => app.$.resultTextContainer.classList.contains('can-scroll'));
    assertEquals(220, app.$.body.offsetHeight);
    assertTrue(
        220 < app.$.resultTextContainer.scrollHeight,
        'Scroll height (' + app.$.resultTextContainer.scrollHeight +
            ' should be bigger than 220.');

    // Results text should not be scrollable when a short response is received.
    app.$.modifierMenu.value = `${StyleModifier.kRetry}`;
    app.$.modifierMenu.dispatchEvent(new CustomEvent('change'));
    await testProxy.whenCalled('rewrite');
    await mockResponse('Refreshed output.');
    await whenCheck(
        app.$.resultTextContainer,
        () => !app.$.resultTextContainer.classList.contains('can-scroll'));
  });

  test('ComposeWithModifierResult', async () => {
    // Submit the input once so that modifier menu is visible.
    mockInput('Input to refresh.');
    app.$.submitButton.click();
    await mockResponse();

    testProxy.resetResolver('rewrite');

    assertTrue(
        isVisible(app.$.modifierMenu), 'Modifier menu should be visible.');
    assertEquals(
        5,
        app.$.modifierMenu.querySelectorAll('option:not([disabled])').length);

    app.$.modifierMenu.value = `${StyleModifier.kShorter}`;
    app.$.modifierMenu.dispatchEvent(new CustomEvent('change'));

    const args = await testProxy.whenCalled('rewrite');
    await mockResponse();

    assertEquals(StyleModifier.kShorter, args);
  });
});

suite('ComposeAppLegacyUi', () => {
  let app: ComposeAppElement;
  let testProxy: TestComposeApiProxy;

  suiteSetup(function() {
    if (loadTimeData.getBoolean('enableUpfrontInputModes')) {
      this.skip();
    }
  });

  setup(async () => {
    testProxy = new TestComposeApiProxy();
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

  test('SavesState', async () => {
    assertEquals(0, testProxy.getCallCount('saveWebuiState'));

    async function assertSavedState(expectedState: ComposeAppState) {
      const savedState = await testProxy.whenCalled('saveWebuiState');
      assertDeepEquals(expectedState, JSON.parse(savedState));
      testProxy.resetResolver('saveWebuiState');
    }

    // Changing input saves state.
    mockInput('Here is my input');
    await assertSavedState(
        {input: 'Here is my input', inputMode: InputMode.kUnset});

    // Visibilitychange event saves state.
    Object.defineProperty(
        document, 'visibilityState', {value: 'hidden', writable: true});
    document.dispatchEvent(new CustomEvent('visibilitychange'));
    await assertSavedState(
        {input: 'Here is my input', inputMode: InputMode.kUnset});

    // Hitting submit saves state.
    app.$.submitButton.click();
    await assertSavedState(
        {input: 'Here is my input', inputMode: InputMode.kUnset});

    // Hitting edit button saves state.
    app.$.textarea.dispatchEvent(
        new CustomEvent('edit-click', {composed: true, bubbles: true}));
    await assertSavedState({
      editedInput: 'Here is my input',
      input: 'Here is my input',
      inputMode: InputMode.kUnset,
      isEditingSubmittedInput: true,
    });

    // Updating edit textarea saves state.
    app.$.editTextarea.value = 'Here is my new input';
    app.$.editTextarea.dispatchEvent(new CustomEvent('value-changed'));
    await assertSavedState({
      editedInput: 'Here is my new input',
      input: 'Here is my input',
      inputMode: InputMode.kUnset,
      isEditingSubmittedInput: true,
    });

    // Canceling reverts state back to before editing.
    app.$.cancelEditButton.click();
    await assertSavedState(
        {input: 'Here is my input', inputMode: InputMode.kUnset});

    // Submitting edited textarea saves state.
    app.$.textarea.dispatchEvent(
        new CustomEvent('edit-click', {composed: true, bubbles: true}));
    app.$.editTextarea.value = 'Here is my new input!!!!';
    app.$.editTextarea.dispatchEvent(new CustomEvent('value-changed'));
    testProxy.resetResolver('saveWebuiState');
    app.$.submitEditButton.click();
    await assertSavedState(
        {input: 'Here is my new input!!!!', inputMode: InputMode.kUnset});
  });

  test('DebouncesSavingState', async () => {
    mockInput('Here is my input');
    mockInput('Here is my input 2');
    await flushTasks();
    const savedState = await testProxy.whenCalled('saveWebuiState');
    assertEquals(1, testProxy.getCallCount('saveWebuiState'));
    assertEquals(
        JSON.stringify(
            {input: 'Here is my input 2', inputMode: InputMode.kUnset}),
        savedState);
  });
});

suite('ComposeAppLegacyInputModesUi', () => {
  let app: ComposeAppElement;
  let testProxy: TestComposeApiProxy;

  suiteSetup(function() {
    if (!loadTimeData.getBoolean('enableUpfrontInputModes')) {
      this.skip();
    }
  });

  setup(async () => {
    testProxy = new TestComposeApiProxy();
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

  test('SavesState', async () => {
    assertEquals(0, testProxy.getCallCount('saveWebuiState'));

    async function assertSavedState(expectedState: ComposeAppState) {
      const savedState = await testProxy.whenCalled('saveWebuiState');
      assertDeepEquals(expectedState, JSON.parse(savedState));
      testProxy.resetResolver('saveWebuiState');
    }

    // Changing input saves state.
    mockInput('Here is my input');
    await assertSavedState(
        {input: 'Here is my input', inputMode: InputMode.kPolish});

    // Visibilitychange event saves state.
    Object.defineProperty(
        document, 'visibilityState', {value: 'hidden', writable: true});
    document.dispatchEvent(new CustomEvent('visibilitychange'));
    await assertSavedState(
        {input: 'Here is my input', inputMode: InputMode.kPolish});

    // Hitting submit saves state.
    app.$.submitButton.click();
    await assertSavedState(
        {input: 'Here is my input', inputMode: InputMode.kPolish});

    // Hitting edit button saves state.
    app.$.textarea.dispatchEvent(
        new CustomEvent('edit-click', {composed: true, bubbles: true}));
    await assertSavedState({
      editedInput: 'Here is my input',
      input: 'Here is my input',
      inputMode: InputMode.kPolish,
      isEditingSubmittedInput: true,
    });

    // Updating edit textarea saves state.
    app.$.editTextarea.value = 'Here is my new input';
    app.$.editTextarea.dispatchEvent(new CustomEvent('value-changed'));
    await assertSavedState({
      editedInput: 'Here is my new input',
      input: 'Here is my input',
      inputMode: InputMode.kPolish,
      isEditingSubmittedInput: true,
    });

    // Canceling reverts state back to before editing.
    app.$.cancelEditButton.click();
    await assertSavedState(
        {input: 'Here is my input', inputMode: InputMode.kPolish});

    // Submitting edited textarea saves state.
    app.$.textarea.dispatchEvent(
        new CustomEvent('edit-click', {composed: true, bubbles: true}));
    app.$.editTextarea.value = 'Here is my new input!!!!';
    app.$.editTextarea.dispatchEvent(new CustomEvent('value-changed'));
    testProxy.resetResolver('saveWebuiState');
    app.$.submitEditButton.click();
    await assertSavedState(
        {input: 'Here is my new input!!!!', inputMode: InputMode.kPolish});
  });

  test('DebouncesSavingState', async () => {
    mockInput('Here is my input');
    mockInput('Here is my input 2');
    await flushTasks();
    const savedState = await testProxy.whenCalled('saveWebuiState');
    assertEquals(1, testProxy.getCallCount('saveWebuiState'));
    assertEquals(
        JSON.stringify(
            {input: 'Here is my input 2', inputMode: InputMode.kPolish}),
        savedState);
  });
});
