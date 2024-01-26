// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://compose/app.js';

import {ComposeAppElement, ComposeAppState} from 'chrome://compose/app.js';
import {CloseReason, ComposeState, ComposeStatus, Length, Tone, UserFeedback} from 'chrome://compose/compose.mojom-webui.js';
import {ComposeApiProxyImpl} from 'chrome://compose/compose_api_proxy.js';
import {CrFeedbackOption} from 'chrome://resources/cr_elements/cr_feedback_buttons/cr_feedback_buttons.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible, whenCheck} from 'chrome://webui-test/test_util.js';

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
      status: ComposeStatus = ComposeStatus.kOk,
      onDeviceEvaluationUsed = false): Promise<void> {
    testProxy.remote.responseReceived(
        {status: status, undoAvailable: false, result, onDeviceEvaluationUsed});
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
    assertTrue(
        appWithTextSelected.$.acceptButton.textContent!.includes('Replace'));

    const appWithNoTextSelected =
        await initializeNewAppWithTextSelectedState(false);
    assertTrue(
        appWithNoTextSelected.$.acceptButton.textContent!.includes('Insert'));
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

    assertEquals(null, args);

    // Verify UI has updated with refreshed results.
    assertFalse(isVisible(app.$.loading));
    assertTrue(
        isVisible(app.$.resultContainer),
        'App result container should be visible.');
    assertTrue(
        app.$.resultContainer.textContent!.includes('Refreshed output.'));
  });

  test('UpdatesScrollableBodyAfterResize', async () => {
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

    // Mock resizing result container down to a 50px height. This should result
    // in the body changing height, triggering the updates to the CSS classes.
    // At this point, 50px is too short to scroll, so it should not have the
    // 'can-scroll' class.
    app.$.resultContainer.style.minHeight = '50px';
    app.$.resultContainer.style.height = '50px';
    app.$.resultContainer.style.overflow = 'hidden';
    await whenCheck(
        app.$.body, () => !app.$.body.classList.contains('can-scroll'));
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
        result: 'here is a result',
        onDeviceEvaluationUsed: false,
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
        onDeviceEvaluationUsed: false,
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
        onDeviceEvaluationUsed: false,
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
        onDeviceEvaluationUsed: false,
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

    await testError(ComposeStatus.kFiltered, 'errorFiltered');
    await testError(ComposeStatus.kRequestThrottled, 'errorRequestThrottled');
    await testError(ComposeStatus.kOffline, 'errorOffline');
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

  test('UnsupportedLanguageErrorClickable', async () => {
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
        2, app.$.lengthMenu.querySelectorAll('option:not([disabled])').length);

    app.$.lengthMenu.value = `${Length.kShorter}`;
    app.$.lengthMenu.dispatchEvent(new CustomEvent('change'));

    const args = await testProxy.whenCalled('rewrite');
    await mockResponse();

    assertEquals(Length.kShorter, args.length);

    testProxy.resetResolver('rewrite');

    assertTrue(isVisible(app.$.toneMenu), 'Tone menu should be visible.');
    assertEquals(
        2, app.$.toneMenu.querySelectorAll('option:not([disabled])').length);

    app.$.toneMenu.value = `${Tone.kCasual}`;
    app.$.toneMenu.dispatchEvent(new CustomEvent('change'));

    const args2 = await testProxy.whenCalled('rewrite');
    await mockResponse();

    assertEquals(Tone.kCasual, args2.tone);
  });

  test('Undo', async () => {
    // Set up initial state to show undo button and mock up a previous state.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testProxy.setOpenMetadata({}, {
      hasPendingRequest: false,
      response: {
        status: ComposeStatus.kOk,
        undoAvailable: true,
        result: 'here is a result',
        onDeviceEvaluationUsed: false,
      },
    });
    testProxy.setUndoResponse({
      hasPendingRequest: false,
      response: {
        status: ComposeStatus.kOk,
        undoAvailable: false,
        result: 'some undone result',
        onDeviceEvaluationUsed: false,
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

  test('PartialResponseIsShown', async () => {
    mockInput('Some fake input.');
    app.$.submitButton.click();
    await testProxy.whenCalled('compose');

    // A partial response is shown.
    await mockPartialResponse('partial response');
    assertTrue(isVisible(app.$.partialResultText));
    assertEquals(app.$.partialResultText.innerText.trim(), 'partial response');

    // The final response hides the partial response text.
    await mockResponse(
        'some response', ComposeStatus.kOk, /*onDeviceEvaluationUsed=*/ true);
    assertFalse(isVisible(app.$.partialResultText));
    assertTrue(isVisible(app.$.onDeviceUsedFooter));
  });
});
