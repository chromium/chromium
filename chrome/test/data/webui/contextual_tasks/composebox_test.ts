// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/app.js';

import type {ContextualTasksAppElement} from 'chrome://contextual-tasks/app.js';
import {BrowserProxyImpl} from 'chrome://contextual-tasks/contextual_tasks_browser_proxy.js';
import type {ComposeboxFile} from 'chrome://resources/cr_components/composebox/common.js';
import {PageCallbackRouter as ComposeboxPageCallbackRouter, PageHandlerRemote as ComposeboxPageHandlerRemote} from 'chrome://resources/cr_components/composebox/composebox.mojom-webui.js';
import {ComposeboxProxyImpl} from 'chrome://resources/cr_components/composebox/composebox_proxy.js';
import {ContextUploadStatus} from 'chrome://resources/cr_components/composebox/composebox_query.mojom-webui.js';
import {GlowAnimationState} from 'chrome://resources/cr_components/search/constants.js';
import {createAutocompleteMatch, createAutocompleteResultForTesting} from 'chrome://resources/cr_components/searchbox/searchbox_browser_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote, type PageRemote as SearchboxPageRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {MockTimer} from 'chrome://webui-test/mock_timer.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestContextualTasksBrowserProxy} from './test_contextual_tasks_browser_proxy.js';
import {ADD_FILE_CONTEXT_FN, assertStyle, mockInputState, setupAutocompleteResults, simulateUserInput, uploadFileAndVerify} from './test_utils.js';

const FAKE_TOKEN_STRING = '00000000000000001234567890ABCDEF';
const FAKE_TOKEN_STRING_2 = '00000000000000001234567890ABCDFF';

type MockContextualTasksAppElement =
    Omit<ContextualTasksAppElement,|'isZeroState_'|'isShownInTab_'>&{
      isZeroState_: boolean,
      isShownInTab_: boolean,
    };

function pressEnter(element: HTMLElement) {
  element.dispatchEvent(new KeyboardEvent('keydown', {
    key: 'Enter',
    bubbles: true,
    composed: true,
  }));
}

suite('ContextualTasksComposeboxTest', () => {
  let contextualTasksApp: MockContextualTasksAppElement;
  let composebox: any;
  let testProxy: TestContextualTasksBrowserProxy;
  let mockComposeboxPageHandler: TestMock<ComposeboxPageHandlerRemote>;
  let mockSearchboxPageHandler: TestMock<SearchboxPageHandlerRemote>;
  let searchboxCallbackRouterRemote: SearchboxPageRemote;
  let mockTimer: MockTimer;

  class MockResizeObserver {
    static instances: MockResizeObserver[] = [];

    constructor(private callback: ResizeObserverCallback) {
      MockResizeObserver.instances.push(this);
    }

    observe(_target: Element) {}
    unobserve(_target: Element) {}
    disconnect() {}

    trigger() {
      // Trigger with empty entries as the component doesn't use entries
      this.callback([], this);
    }
  }

  setup(async () => {
    const win = window as any;

    if (!win.chrome) {
      win.chrome = {};
    }

    if (!win.chrome.histograms) {
      win.chrome.histograms = {
        recordEnumerationValue: () => {},
        recordUserAction: () => {},
        recordBoolean: () => {},
      };
    }
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    // Mock ResizeObserver
    window.ResizeObserver = MockResizeObserver;
    MockResizeObserver.instances = [];

    mockTimer = new MockTimer();

    loadTimeData.overrideValues({
      contextualMenuUsePecApi: false,
      composeboxShowTypedSuggest: true,
      composeboxShowZps: true,
      enableBasicModeZOrder: true,
      composeboxShowContextMenu: true,
      composeboxHintTextLensOverlay: 'Test Lens Hint',
    });

    testProxy = new TestContextualTasksBrowserProxy('https://google.com');
    BrowserProxyImpl.setInstance(testProxy);

    mockComposeboxPageHandler = TestMock.fromClass(ComposeboxPageHandlerRemote);
    mockSearchboxPageHandler = TestMock.fromClass(SearchboxPageHandlerRemote);
    mockSearchboxPageHandler.setResultFor(
        'getRecentTabs', Promise.resolve({tabs: []}));
    mockSearchboxPageHandler.setResultFor('getInputState', Promise.resolve({
      state: {
        allowedModels: [],
        allowedTools: [],
        allowedInputTypes: [],
        activeModel: 0,
        activeTool: 0,
        disabledModels: [],
        disabledTools: [],
        disabledInputTypes: [],
      },
    }));
    const searchboxCallbackRouter = new SearchboxPageCallbackRouter();
    searchboxCallbackRouterRemote =
        searchboxCallbackRouter.$.bindNewPipeAndPassRemote();
    ComposeboxProxyImpl.setInstance(new ComposeboxProxyImpl(
        mockComposeboxPageHandler as any, new ComposeboxPageCallbackRouter(),
        mockSearchboxPageHandler as any, searchboxCallbackRouter));

    contextualTasksApp = document.createElement('contextual-tasks-app') as
        unknown as MockContextualTasksAppElement;
    document.body.appendChild(contextualTasksApp);
    await microtasksFinished();
    composebox = contextualTasksApp.$.composebox.$.composebox;

    assertTrue(
        MockResizeObserver.instances.length >= 1,
        'There should be at least one ResizeObserver instance.');

    searchboxCallbackRouterRemote.onInputStateChanged(mockInputState);
    await microtasksFinished();
  });

  teardown(() => {
    mockTimer.uninstall();
  });

  async function deleteLastFile() {
    const files = composebox.$.carousel.files;
    const deletedId = files[files.length - 1]!.uuid;
    composebox.$.carousel.dispatchEvent(
        new CustomEvent('delete-file', {
          detail: {
            uuid: deletedId,
          },
          bubbles: true,
          composed: true,
        }));
    await microtasksFinished();
  }
  function getSubmitContainer(): HTMLElement|null {
    return composebox.shadowRoot.querySelector('#submitContainer');
  }

  function getSubmitButton(): HTMLButtonElement|null {
    const submitContainer: HTMLElement|null = getSubmitContainer();

    if (!submitContainer) {
      return null;
    }

    const submitButton: HTMLButtonElement|null =
        submitContainer.querySelector('#submitIcon');
    return submitButton;
  }
  test(
      'Upload status is tracked properly when adding file via browser',
      async () => {
        const fileInfo = {
          fileName: 'test-image.png',
          imageDataUrl: 'data:image/png;base64,xxxx',
          isDeletable: true,
        };
        mockSearchboxPageHandler.setResultFor(
            ADD_FILE_CONTEXT_FN, Promise.resolve({token: FAKE_TOKEN_STRING}));
        composebox.addFileContextFromBrowser_(FAKE_TOKEN_STRING, fileInfo);

        searchboxCallbackRouterRemote.onContextualInputStatusChanged(
            FAKE_TOKEN_STRING,
            ContextUploadStatus.kProcessingSuggestSignalsReady,
            /*error_type=*/ null,
        );

        await composebox.updateComplete;
        await microtasksFinished();

        const remaining = composebox.getRemainingFilesToUpload();

        assertEquals(1, remaining.size, 'Pending uploads should increase');
        assertTrue(
            remaining.has(FAKE_TOKEN_STRING),
            'The set should contain our specific UUID');

        assertFalse(
            composebox.fileUploadsComplete,
            'fileUploadsComplete should be false');

        const submitButton: HTMLButtonElement|null = getSubmitButton();

        assertTrue(!!submitButton, 'Submit button should exist');
        assertTrue(submitButton?.disabled, 'Submit button should be disabled');
        // Simulate tab upload success.
        searchboxCallbackRouterRemote.onContextualInputStatusChanged(
            FAKE_TOKEN_STRING,
            ContextUploadStatus.kUploadSuccessful,
            /*error_type=*/ null,
        );

        await composebox.updateComplete;
        await microtasksFinished();

        await microtasksFinished();
        await composebox.updateComplete;
        const submitContainer: HTMLElement|null = getSubmitContainer();
        assertTrue(!!submitContainer, 'Submit container button should exist');
        assertFalse(
            submitButton?.disabled, 'Submit button should not be disabled');

        assertStyle(
            submitButton, 'pointer-events', 'auto',
            'Submit button should not be disabled');
        assertStyle(
            submitContainer, 'cursor', 'pointer',
            'Submit button cursor should be pointer');
        assertTrue(!!submitContainer, 'Submit container button should exist');

        submitContainer?.click();

        // Flush the macrotask queue / event loop
        await new Promise(resolve => setTimeout(resolve, 0));
        await composebox.updateComplete;
        await microtasksFinished();

        assertEquals(0, composebox.files_.size);

        // Should be no longer `EXPANDING` after successful upload and submit
        // click.
        assertNotEquals(
            composebox.animationState, GlowAnimationState.EXPANDING);
      });

  test(
      'Upload status is tracked properly when adding and removing files',
      async () => {
        assertEquals(0, composebox.getRemainingFilesToUpload().size);
        const testFile1 = new File(['test'], 'test1.jpg', {type: 'image/jpeg'});
        await uploadFileAndVerify(
            FAKE_TOKEN_STRING, testFile1, composebox, mockSearchboxPageHandler);

        searchboxCallbackRouterRemote.onContextualInputStatusChanged(
            FAKE_TOKEN_STRING,
            ContextUploadStatus.kNotUploaded,
            /*error_type=*/ null,
        );

        await composebox.updateComplete;
        await microtasksFinished();

        assertEquals(
            0, composebox.getRemainingFilesToUpload().size,
            'First file should be uploading.');
        assertTrue(
            composebox.fileUploadsComplete,
            'Files should not be finished uploading (first file)');

        searchboxCallbackRouterRemote.onContextualInputStatusChanged(
            FAKE_TOKEN_STRING,
            ContextUploadStatus.kProcessing,
            /*error_type=*/ null,
        );

        await composebox.updateComplete;
        await microtasksFinished();

        assertEquals(
            1, composebox.getRemainingFilesToUpload().size,
            'First file should be uploading.');
        assertFalse(
            composebox.fileUploadsComplete,
            'Files should not be finished uploading (first file)');
        const testFile2 =
            new File(['test2'], 'test2.jpg', {type: 'image/jpeg'});
        await uploadFileAndVerify(
            FAKE_TOKEN_STRING_2, testFile2, composebox,
            mockSearchboxPageHandler, 1);

        searchboxCallbackRouterRemote.onContextualInputStatusChanged(
            FAKE_TOKEN_STRING_2,
            ContextUploadStatus.kProcessingSuggestSignalsReady,
            /*error_type=*/ null,
        );

        await composebox.updateComplete;
        await microtasksFinished();

        assertEquals(
            2, composebox.getRemainingFilesToUpload().size,
            'Second file should be uploading');
        assertFalse(
            composebox.fileUploadsComplete,
            'Files should not be finished uploading (second file)');

        await deleteLastFile();
        assertEquals(
            1, composebox.getRemainingFilesToUpload().size,
            'File should be deleted and number of files left are 1');

        await deleteLastFile();
        assertEquals(
            0, composebox.getRemainingFilesToUpload().size,
            'File should be deleted and number of files left are 0');
      });

  test('clear all (cancel) works for uploading set', async () => {
    const token = FAKE_TOKEN_STRING;
    await uploadFileAndVerify(
        token, new File(['foo'], 'foo.jpg', {type: 'image/jpeg'}), composebox,
        mockSearchboxPageHandler);

    await composebox.updateComplete;
    await microtasksFinished();

    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        token, ContextUploadStatus.kUploadSuccessful, null);

    await searchboxCallbackRouterRemote.$.flushForTesting();
    await composebox.updateComplete;
    await microtasksFinished();

    composebox.clearAllInputs(false);

    await Promise.all([
      composebox.updateComplete,
      microtasksFinished(),
    ]);

    assertEquals(0, composebox.files_.size);

    const submitButton: HTMLButtonElement|null = getSubmitButton();
    assertTrue(!!submitButton, 'Submit button should exist');
    assertTrue(submitButton?.disabled, 'Button should be disabled');

    const submitContainer: HTMLElement|null = getSubmitContainer();
    assertTrue(!!submitContainer, 'Submit container button should exist');

    assertStyle(
        submitContainer, 'cursor', 'not-allowed',
        'Submit button cursor should be not-allowed');
    assertStyle(
        submitContainer, 'pointer-events', 'auto',
        'Submit container should still have pointer-events on,\
            even when disabled.');

    assertEquals(0, composebox.getRemainingFilesToUpload().size);
  });

  test(
      'clear all (cancel) works for uploading set with undeletable files',
      async () => {
        const token1 = FAKE_TOKEN_STRING;
        await uploadFileAndVerify(
            token1, new File(['foo'], 'foo.jpg', {type: 'image/jpeg'}),
            composebox, mockSearchboxPageHandler);

        const currentFiles = composebox.files_;
        currentFiles.forEach((file: ComposeboxFile) => {
          file.isDeletable = false;
        });

        composebox.requestUpdate();

        await composebox.updateComplete;
        await microtasksFinished();

        // Now file 1 is not deletable while file 2 is.
        const token2 = FAKE_TOKEN_STRING_2;
        await uploadFileAndVerify(
            token2, new File(['foo2'], 'foo2.jpg', {type: 'image/png'}),
            composebox, mockSearchboxPageHandler, 1);
        searchboxCallbackRouterRemote.onContextualInputStatusChanged(
            token1, ContextUploadStatus.kUploadSuccessful, null);
        await searchboxCallbackRouterRemote.$.flushForTesting();

        searchboxCallbackRouterRemote.onContextualInputStatusChanged(
            token2, ContextUploadStatus.kUploadSuccessful, null);
        await searchboxCallbackRouterRemote.$.flushForTesting();

        await composebox.updateComplete;
        await microtasksFinished();

        // Clear all inputs (only deletes file 2).
        composebox.clearAllInputs(false);
        await composebox.updateComplete;
        await microtasksFinished();

        assertEquals(1, composebox.getRemainingFilesToUpload().size);

        const submitButton: HTMLButtonElement|null = getSubmitButton();
        const submitContainer: HTMLElement|null = getSubmitContainer();
        assertTrue(!!submitButton, 'Submit button should exist');

        // There are no more deletable files, so submit should be disabled.
        assertTrue(submitButton?.disabled, 'Button should be disabled');

        assertTrue(!!submitContainer, 'Submit container button should exist');

        assertStyle(
            submitContainer, 'cursor', 'not-allowed',
            'Submit button cursor should be not-allowed');
        assertStyle(
            submitContainer, 'pointer-events', 'auto',
            'Submit container should still have pointer-events on,\
                even when disabled.');

        // Reupload 2nd deleted file.
        await uploadFileAndVerify(
            token2, new File(['foo3'], 'foo3.jpg', {type: 'image/png'}),
            composebox, mockSearchboxPageHandler, 1);

        searchboxCallbackRouterRemote.onContextualInputStatusChanged(
            token2, ContextUploadStatus.kUploadSuccessful, null);
        await searchboxCallbackRouterRemote.$.flushForTesting();
        await composebox.updateComplete;
        await microtasksFinished();

        const currentFiles2 = composebox.files_;
        currentFiles2.forEach((file: ComposeboxFile) => {
          file.isDeletable = false;
        });

        composebox.requestUpdate();

        await composebox.updateComplete;
        await microtasksFinished();

        // Clear all inputs (deletes no files).
        composebox.clearAllInputs(false);
        await composebox.updateComplete;
        await microtasksFinished();
        assertEquals(2, composebox.getRemainingFilesToUpload().size);

        assertTrue(!!submitButton, 'Submit button should exist');
        // There are no more deletable files, so submit should be disabled.
        assertTrue(submitButton?.disabled, 'Button should be disabled');

        assertTrue(!!submitContainer, 'Submit container button should exist');

        assertStyle(
            submitContainer, 'cursor', 'not-allowed',
            'Submit button cursor should be not-allowed');
        assertStyle(
            submitContainer, 'pointer-events', 'auto',
            'Submit container should still have pointer-events on,\
                even when disabled.');
        assertEquals(2, composebox.getRemainingFilesToUpload().size);
      });

  test('FocusUpdatesProperty', () => {
    mockTimer.install();
    const composebox = contextualTasksApp.$.composebox;
    const innerComposebox = composebox.$.composebox;

    innerComposebox.dispatchEvent(new CustomEvent('composebox-focus-in'));
    mockTimer.tick(0);  // Attribute reflection is async
    assertTrue(composebox.isComposeboxFocusedForTesting);

    innerComposebox.dispatchEvent(new CustomEvent('composebox-focus-out'));
    assertTrue(!composebox.isComposeboxFocusedForTesting);
  });

  test('ResizeUpdatesHeight', () => {
    mockTimer.install();
    const composebox = contextualTasksApp.$.composebox;
    const innerComposebox = composebox.$.composebox;


    innerComposebox.style.display = 'block';
    innerComposebox.style.height = '100px';


    Object.defineProperty(innerComposebox, 'offsetHeight', {
      writable: true,
      configurable: true,
      value: 100,
    });

    MockResizeObserver.instances.forEach(obs => obs.trigger());
    mockTimer.tick(100);

    const height1 = composebox.composeboxHeightForTesting;
    assertTrue(typeof height1 === 'number');
    assertTrue(height1 > 0, `height1 should be > 0, but is ${height1}`);

    innerComposebox.style.height = '300px';

    // Update mock
    Object.defineProperty(innerComposebox, 'offsetHeight', {
      writable: true,
      configurable: true,
      value: 300,
    });

    MockResizeObserver.instances.forEach(obs => obs.trigger());
    mockTimer.tick(100);

    const height2 = composebox.composeboxHeightForTesting;
    assertTrue(typeof height2 === 'number');
    assertTrue(
        height1 !== height2, `Height should change: ${height1} vs ${height2}`);
  });

  test('SelectingMatchPopulatesComposebox', async () => {
    mockTimer.install();
    const composebox = contextualTasksApp.$.composebox.$.composebox;
    const inputElement = composebox.$.input;


    const testQuery = 'test';
    simulateUserInput(inputElement, testQuery);

    const matches = [
      createAutocompleteMatch({fillIntoEdit: 'match 1'}),
      createAutocompleteMatch({fillIntoEdit: 'match 2'}),
    ];
    searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting(
            {input: testQuery, matches: matches}));
    await searchboxCallbackRouterRemote.$.flushForTesting();
    mockTimer.tick(0);

    const matchesEl = composebox.getMatchesElement();
    assertTrue(!!matchesEl.result, 'Matches should be populated');
    assertEquals(2, matchesEl.result.matches.length, 'Should have 2 matches');


    inputElement.dispatchEvent(new KeyboardEvent(
        'keydown', {key: 'ArrowDown', bubbles: true, composed: true}));

    // Wait for Lit updates to propagate
    mockTimer.tick(100);
    await composebox.getMatchesElement().updateComplete;
    await composebox.updateComplete;
    assertEquals(
        0, composebox.getMatchesElement().selectedMatchIndex,
        'Index should be 0');
    assertEquals(
        'match 1', inputElement.value, 'Input value should be match 1');
    assertEquals(
        0, (composebox as any).selectedMatchIndex_, 'Parent index should be 0');
  });

  test('TooltipVisibilityUpdatesOnResize', () => {
    mockTimer.install();
    const composeboxElement = contextualTasksApp.$.composebox;
    const tooltip = composeboxElement.$.onboardingTooltip;

    // Force show tooltip
    loadTimeData.overrideValues({
      showOnboardingTooltip: true,
      isOnboardingTooltipDismissCountBelowCap: true,
      composeboxShowOnboardingTooltipSessionImpressionCap: 10,
    });
    composeboxElement.numberOfTimesTooltipShownForTesting = 0;
    composeboxElement.userDismissedTooltipForTesting = false;

    // Simulate active tab chip token presence
    const innerComposebox = composeboxElement.$.composebox;
    innerComposebox.getHasAutomaticActiveTabChipToken = () => true;
    innerComposebox.getAutomaticActiveTabChipElement = () =>
        document.createElement('div');

    composeboxElement.updateTooltipVisibilityForTesting();
    assertTrue(tooltip.shouldShow);

    // Resize event
    const resizeEvent = new CustomEvent('composebox-resize', {
      detail: {carouselHeight: 50},
      bubbles: true,
      composed: true,
    });
    innerComposebox.dispatchEvent(resizeEvent);

    // Tooltip should still be shown and position updated (implicitly via resize
    // observer or logic)
    assertTrue(tooltip.shouldShow);
  });

  test('TooltipImpressionIncrementsAfterDelay', () => {
    mockTimer.install();
    const composeboxElement = contextualTasksApp.$.composebox;
    const tooltip = composeboxElement.$.onboardingTooltip;

    // Force show tooltip with delay.
    loadTimeData.overrideValues({
      showOnboardingTooltip: true,
      isOnboardingTooltipDismissCountBelowCap: true,
      composeboxShowOnboardingTooltipSessionImpressionCap: 10,
      composeboxShowOnboardingTooltipImpressionDelay: 3000,
    });
    composeboxElement.numberOfTimesTooltipShownForTesting = 0;
    composeboxElement.userDismissedTooltipForTesting = false;

    const innerComposebox = composeboxElement.$.composebox;
    innerComposebox.getHasAutomaticActiveTabChipToken = () => true;
    innerComposebox.getAutomaticActiveTabChipElement = () =>
        document.createElement('div');

    // Trigger update.
    composeboxElement.updateTooltipVisibilityForTesting();
    assertTrue(tooltip.shouldShow);

    // Should not have incremented yet.
    assertEquals(0, composeboxElement.numberOfTimesTooltipShownForTesting);

    // Tick almost to the end.
    mockTimer.tick(2999);
    assertEquals(0, composeboxElement.numberOfTimesTooltipShownForTesting);

    // Tick past the delay.
    mockTimer.tick(1);
    assertEquals(1, composeboxElement.numberOfTimesTooltipShownForTesting);
  });

  test('TooltipImpressionTimerResetsOnHide', () => {
    mockTimer.install();
    const composeboxElement = contextualTasksApp.$.composebox;
    const tooltip = composeboxElement.$.onboardingTooltip;

    loadTimeData.overrideValues({
      showOnboardingTooltip: true,
      isOnboardingTooltipDismissCountBelowCap: true,
      composeboxShowOnboardingTooltipSessionImpressionCap: 10,
      composeboxShowOnboardingTooltipImpressionDelay: 3000,
    });
    composeboxElement.numberOfTimesTooltipShownForTesting = 0;
    composeboxElement.userDismissedTooltipForTesting = false;

    const innerComposebox = composeboxElement.$.composebox;
    // Mock existence of chip.
    innerComposebox.getHasAutomaticActiveTabChipToken = () => true;
    innerComposebox.getAutomaticActiveTabChipElement = () =>
        document.createElement('div');

    // Show tooltip.
    composeboxElement.updateTooltipVisibilityForTesting();
    assertTrue(tooltip.shouldShow);

    // Advance time partially.
    mockTimer.tick(1000);
    assertEquals(0, composeboxElement.numberOfTimesTooltipShownForTesting);

    // Hide tooltip (e.g. chip disappears).
    innerComposebox.getHasAutomaticActiveTabChipToken = () => false;
    composeboxElement.updateTooltipVisibilityForTesting();
    assertFalse(tooltip.shouldShow);

    // Advance past original deadline.
    mockTimer.tick(5000);
    // Should NOT have incremented because timer was cleared.
    assertEquals(0, composeboxElement.numberOfTimesTooltipShownForTesting);
  });

  test(
      'on focus out does not set animation state as none \
      when submitting or listening',
      async () => {
        const composebox = contextualTasksApp.$.composebox.$.composebox;

        composebox.animationState = GlowAnimationState.SUBMITTING;
        composebox.dispatchEvent(new CustomEvent('composebox-focus-out', {
          bubbles: true,
          composed: true,
        }));
        await microtasksFinished();
        assertEquals(composebox.animationState, GlowAnimationState.SUBMITTING);

        composebox.animationState = GlowAnimationState.LISTENING;
        composebox.dispatchEvent(new CustomEvent('composebox-focus-out', {
          bubbles: true,
          composed: true,
        }));
        await microtasksFinished();
        assertEquals(composebox.animationState, GlowAnimationState.LISTENING);
      });

  test('on focus out sets animation state as none otherwise', async () => {
    const composebox = contextualTasksApp.$.composebox.$.composebox;
    composebox.animationState = GlowAnimationState.EXPANDING;
    composebox.dispatchEvent(new CustomEvent('composebox-focus-out', {
      bubbles: true,
      composed: true,
    }));
    await microtasksFinished();
    assertEquals(composebox.animationState, GlowAnimationState.NONE);
  });

  test(
      'side panel handles AIM queries to show side panel zero state correctly',
      async () => {
        contextualTasksApp.isShownInTab_ = false;
        contextualTasksApp.isZeroState_ = false;

        await contextualTasksApp.updateComplete;
        await contextualTasksApp.$.composebox.updateComplete;
        await microtasksFinished();

        assertStyle(
            contextualTasksApp.$.composeboxHeader, 'font-size', '28px',
            'When in side panel non-zero-state, composebox header font-size');
        assertStyle(
            contextualTasksApp.$.composebox.$.composebox, 'min-width', '200px',
            'When in side panel non-zero-state, composebox min-width');

        contextualTasksApp.isZeroState_ = true;
        await contextualTasksApp.updateComplete;
        await contextualTasksApp.$.composebox.updateComplete;
        await microtasksFinished();

        assertStyle(
            contextualTasksApp.$.composebox.$.composeboxContainer, 'position',
            'relative');
        assertStyle(
            contextualTasksApp.$.composebox.$.composeboxContainer,
            'margin-bottom', '0px',
            'When in side panel zero-state, composebox wrapper margin');
        assertStyle(
            contextualTasksApp.$.composebox, 'position', 'relative',
            'When in side panel zero-state, composebox position');

        contextualTasksApp.isZeroState_ = false;
        await contextualTasksApp.updateComplete;
        await contextualTasksApp.$.composebox.updateComplete;
        await microtasksFinished();

        assertStyle(
            contextualTasksApp.$.composebox.$.composeboxContainer, 'position',
            'relative', 'When returning to side panel non-zero-state,\
                composebox wrapper position');
        assertStyle(
            contextualTasksApp.$.composebox.$.composeboxContainer,
            'margin-bottom', '30px',
            'When returning to side panel non-zero-state,\
                composebox wrapper margin');
        assertStyle(
            contextualTasksApp.$.composebox, 'position', 'relative',
            'When returning to side panel non-zero-state,\
                composebox position');
      });

  test('full tab handles AIM queries to show 0 state correctly', async () => {
    contextualTasksApp.isShownInTab_ = true;
    contextualTasksApp.isZeroState_ = false;
    await contextualTasksApp.updateComplete;
    await contextualTasksApp.$.composebox.updateComplete;
    await microtasksFinished();

    assertStyle(
        contextualTasksApp.$.composebox.$.composebox, 'min-width', '0px',
        'When in full tab mode non-zero-state, composebox min-width');
    assertStyle(
        contextualTasksApp.$.composeboxHeader, 'font-size', '32px',
        'When in full tab mode non-zero-state, composebox header font-size');

    contextualTasksApp.isZeroState_ = true;
    await contextualTasksApp.updateComplete;
    await contextualTasksApp.$.composebox.updateComplete;
    await microtasksFinished();

    assertStyle(
        contextualTasksApp.$.composebox.$.composeboxContainer, 'position',
        'relative',
        'When in full tab mode zero-state, composebox wrapper position');
    assertStyle(
        contextualTasksApp.$.composebox.$.composeboxContainer, 'margin-bottom',
        '0px', 'When in full tab mode zero-state, composebox wrapper margin');
    assertStyle(
        contextualTasksApp.$.composebox, 'position', 'relative',
        'When in full tab mode zero-state, composebox wrapper position');

    contextualTasksApp.isZeroState_ = false;
    await contextualTasksApp.updateComplete;
    await contextualTasksApp.$.composebox.updateComplete;
    await microtasksFinished();

    assertStyle(
        contextualTasksApp.$.composebox.$.composeboxContainer, 'position',
        'relative', 'When returning to full tab mode non-zero-state,\
            composebox wrapper position');
    assertStyle(
        contextualTasksApp.$.composebox.$.composeboxContainer, 'margin-bottom',
        '30px', 'When returning to full tab non-zero-state,\
            composebox wrapper margin');
    assertStyle(
        contextualTasksApp.$.composebox, 'position', 'relative',
        'When returning to full tab mode non-zero-state,\
            composebox position');
  });

  test('EnterKeyAfterSubmitDoesNotAddNewLine', async () => {
    mockTimer.install();
    const TEST_QUERY = 'test query';

    const inputElement = composebox.$.input;
    assertTrue(
        isVisible(inputElement), 'Composebox input element should be visible');

    // 1. Setup: Input text.
    simulateUserInput(inputElement, TEST_QUERY);

    // Advance timer to trigger the debounced autocomplete query.
    mockTimer.tick(300);

    // Wait for the component to actually request autocomplete
    // before we mock the response, otherwise the response is ignored.
    await mockSearchboxPageHandler.whenCalled('queryAutocomplete');

    // 2. Mock Autocomplete Results.
    await setupAutocompleteResults(searchboxCallbackRouterRemote, TEST_QUERY,
        mockTimer);

    // Wait for the matches to be populated in the dropdown.
    while (!composebox.getMatchesElement().result) {
      mockTimer.tick(10);
      await Promise.resolve();
    }

    const submitButton = getSubmitButton();
    assertTrue(!!submitButton);
    assertFalse(submitButton.disabled, 'Submit should be enabled');

    // 3. Action: Simulate Enter press to submit
    mockSearchboxPageHandler.reset();
    pressEnter(inputElement);

    await mockSearchboxPageHandler.whenCalled('openAutocompleteMatch');
    assertEquals(
        1, mockSearchboxPageHandler.getCallCount('openAutocompleteMatch'));

    // Tick timer to allow Lit's update lifecycle to process the submit.
    mockTimer.tick(0);

    // 4. Wait for the UI to clear the input after submission.
    await composebox.updateComplete;
    await contextualTasksApp.updateComplete;
    assertEquals(
        '', inputElement.value, 'Input should be cleared after submit');

    // 5. Action: Press Enter again on empty input.
    mockSearchboxPageHandler.reset();
    pressEnter(inputElement);

    // Tick timer again for Lit updates.
    mockTimer.tick(0);
    await composebox.updateComplete;

    // 6. Assert: No newline added, submit not called again.
    assertFalse(inputElement.value.includes('\n'));
    assertEquals(0, mockSearchboxPageHandler.getCallCount('submitQuery'));
    assertEquals(
        0, mockSearchboxPageHandler.getCallCount('openAutocompleteMatch'));
  });

  test('EnterKeyOnEmptyInputDoesNotAddNewLineOrSubmit', async () => {
    const innerComposebox = contextualTasksApp.$.composebox.$.composebox;
    const inputElement = innerComposebox.$.input;
    const keydownDiv =
        innerComposebox.shadowRoot.querySelector<HTMLElement>('#composebox');
    assertTrue(!!keydownDiv);

    assertEquals('', inputElement.value);
    mockSearchboxPageHandler.reset();

    // Action: Press Enter on empty input.
    pressEnter(keydownDiv);
    await microtasksFinished();

    // Assert: No newline and no submission.
    assertFalse(inputElement.value.includes('\n'));
    assertEquals(0, mockSearchboxPageHandler.getCallCount('submitQuery'));
  });

  test('Composebox upload disabled when uploading files', async () => {
    composebox.searchboxLayoutMode = '';
    composebox.contextMenuEnabled_ = true;
    await composebox.updateComplete;
    await composebox.updateComplete;
    await microtasksFinished();

    const contextEntrypoint =
        composebox.shadowRoot.querySelector('#contextEntrypoint');
    assertTrue(!!contextEntrypoint);
    assertFalse(
        contextEntrypoint.uploadButtonDisabled,
        'Upload button should be enabled');

    await uploadFileAndVerify(
        FAKE_TOKEN_STRING, new File(['foo'], 'foo.jpg', {type: 'image/jpeg'}),
        composebox, mockSearchboxPageHandler);

    // Other processing state should result in not ready to submit.
    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        FAKE_TOKEN_STRING,
        ContextUploadStatus.kProcessingSuggestSignalsReady,
        /*error_type=*/ null,
    );

    await microtasksFinished();
    await composebox.updateComplete;
    assertEquals(
        1, composebox.getRemainingFilesToUpload().size,
        '1 File should be uploading');
    assertFalse(
        composebox.fileUploadsComplete,
        'Files should not be finished uploading');
    assertTrue(
        contextEntrypoint.uploadButtonDisabled,
        'Upload button should be disabled while uploading');

    searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        FAKE_TOKEN_STRING,
        ContextUploadStatus.kUploadSuccessful,
        /*error_type=*/ null,
    );

    await microtasksFinished();
    await composebox.updateComplete;

    assertEquals(
        0, composebox.getRemainingFilesToUpload().size,
        '0 Files should be uploading');
    assertTrue(
        composebox.fileUploadsComplete, 'Files should be finished uploading');
    assertFalse(
        contextEntrypoint.uploadButtonDisabled,
        'Upload button should be re-enabled after upload');
  });

  test(
      'Composebox upload disabled when uploading files with contextMenu',
      async () => {
        composebox.searchboxLayoutMode = '';
        composebox.contextMenuEnabled_ = true;
        await composebox.updateComplete;
        await microtasksFinished();

        const contextEntrypoint =
            composebox.shadowRoot.querySelector('#contextEntrypoint');
        assertTrue(!!contextEntrypoint);

        const button =
            contextEntrypoint.shadowRoot?.querySelector('#entrypointButton');
        assertTrue(!!button, 'Context menu button should exist');

        assertFalse(
            contextEntrypoint.uploadButtonDisabled,
            'Context menu button should be enabled at first');

        await uploadFileAndVerify(
            FAKE_TOKEN_STRING,
            new File(['foo'], 'foo.jpg', {type: 'image/jpeg'}), composebox,
            mockSearchboxPageHandler);

        // Other processing state should result in not ready to submit.
        searchboxCallbackRouterRemote.onContextualInputStatusChanged(
            FAKE_TOKEN_STRING,
            ContextUploadStatus.kProcessingSuggestSignalsReady,
            /*error_type=*/ null,
        );

        await composebox.updateComplete;
        await microtasksFinished();

        assertEquals(
            1, composebox.getRemainingFilesToUpload().size,
            '1 File should be uploading');
        assertFalse(
            composebox.fileUploadsComplete,
            'Files should not be finished uploading');

        assertTrue(
            contextEntrypoint.uploadButtonDisabled,
            'Context menu button should be disabled while uploading');
        searchboxCallbackRouterRemote.onContextualInputStatusChanged(
            FAKE_TOKEN_STRING,
            ContextUploadStatus.kUploadSuccessful,
            /*error_type=*/ null,
        );

        await microtasksFinished();
        await composebox.updateComplete;

        assertEquals(
            0, composebox.getRemainingFilesToUpload().size,
            '0 Files should be uploading');
        assertTrue(
            composebox.fileUploadsComplete,
            'Files should be finished uploading');
        assertFalse(
            contextEntrypoint.uploadButtonDisabled,
            'Context menu button should be enabled after upload');
      });

  test('image upload calls handler for image', async () => {
    const contextEntrypoint =
        composebox.shadowRoot.querySelector('#contextEntrypoint');
    assertTrue(!!contextEntrypoint);
    contextEntrypoint.dispatchEvent(
        new CustomEvent('open-image-upload', {
          detail: {isImage: true},
          bubbles: true,
          composed: true,
        }));

    await mockComposeboxPageHandler.whenCalled('handleFileUpload');
    assertEquals(1, mockComposeboxPageHandler.getCallCount('handleFileUpload'));
    const [isImage] = mockComposeboxPageHandler.getArgs('handleFileUpload');
    assertTrue(isImage);
  });

  test('file upload calls handler for file', async () => {
    const contextEntrypoint =
        composebox.shadowRoot.querySelector('#contextEntrypoint');
    assertTrue(!!contextEntrypoint);
    contextEntrypoint.dispatchEvent(
        new CustomEvent('open-file-upload', {
          detail: {isImage: false},
          bubbles: true,
          composed: true,
        }));

    await mockComposeboxPageHandler.whenCalled('handleFileUpload');
    assertEquals(1, mockComposeboxPageHandler.getCallCount('handleFileUpload'));
    const [isImage] = mockComposeboxPageHandler.getArgs('handleFileUpload');
    assertFalse(isImage);
  });

  test('queries autocomplete on load when isZeroState is true', async () => {
    // Clear the body and reset the mock to test a fresh instance.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    mockSearchboxPageHandler.reset();
    mockSearchboxPageHandler.setResultFor('getInputState', Promise.resolve({
      state: {
        allowedModels: [],
        allowedTools: [],
        allowedInputTypes: [],
        activeModel: 0,
        activeTool: 0,
        disabledModels: [],
        disabledTools: [],
        disabledInputTypes: [],
      },
    }));

    loadTimeData.overrideValues({composeboxShowZps: false});

    const app = document.createElement('contextual-tasks-app') as unknown as
        MockContextualTasksAppElement;
    app.isZeroState_ = false;
    document.body.appendChild(app);
    await app.updateComplete;
    await microtasksFinished();

    // Reset so that way any calls that happen before
    // adding to document do not count (since before that,
    // we are just setting up the test).
    mockSearchboxPageHandler.reset();

    // Mock `isZeroState_` updating value from parent.
    app.isZeroState_ = true;
    await microtasksFinished();

    assertEquals(1, mockSearchboxPageHandler.getCallCount('queryAutocomplete'));
  });

  test(
      'lens button visibility depends on whether DeepSearch is selected in nextbox',
      async () => {
        const contextualComposebox = contextualTasksApp.$.composebox;
        const innerComposebox = contextualComposebox.$.composebox;

        // Ensure we are in side panel mode
        contextualComposebox.isSidePanel = true;
        await contextualComposebox.updateComplete;
        await innerComposebox.updateComplete;
        await microtasksFinished();

        const getLensIcon = () =>
            innerComposebox.shadowRoot.querySelector('#lensIcon');

        assertTrue(
            isVisible(getLensIcon()),
            'Lens button should be visible initially');
        assertTrue(
            innerComposebox.showLensButton,
            'Child showLensButton should be true initially');

        // Enable Deep Search
        innerComposebox.dispatchEvent(
            new CustomEvent('active-tool-mode-changed', {
              bubbles: true,
              composed: true,
              detail: {value: 1},
            }));

        await microtasksFinished();
        await contextualComposebox.updateComplete;
        await innerComposebox.updateComplete;

        // Check the effect
        assertEquals(
            null, getLensIcon(),
            'Lens button should be hidden when Deep Search is active');

        // Disable Deep Search
        innerComposebox.dispatchEvent(
            new CustomEvent('active-tool-mode-changed', {
              bubbles: true,
              composed: true,
              detail: {value: 0},
            }));

        await microtasksFinished();
        await contextualComposebox.updateComplete;
        await innerComposebox.updateComplete;

        // Check the effect
        assertTrue(
            isVisible(getLensIcon()), 'Lens button should be visible again');
      });

  test(
      'does not query autocomplete on load when isZeroState is false',
      async () => {
        // Clear the body and reset the mock to test a fresh instance.
        document.body.innerHTML = window.trustedTypes!.emptyHTML;
        mockSearchboxPageHandler.reset();
        mockSearchboxPageHandler.setResultFor('getInputState', Promise.resolve({
          state: {
            allowedModels: [],
            allowedTools: [],
            allowedInputTypes: [],
            activeModel: 0,
            activeTool: 0,
            disabledModels: [],
            disabledTools: [],
            disabledInputTypes: [],
          },
        }));

        loadTimeData.overrideValues({composeboxShowZps: false});

        const app = document.createElement('contextual-tasks-app') as unknown as
            MockContextualTasksAppElement;
        app.isZeroState_ = false;
        document.body.appendChild(app);

        await app.updateComplete;
        await microtasksFinished();

        // Reset so that way any calls that happen before
        // adding to document do not count (since before that,
        // we are just setting up the test).
        mockSearchboxPageHandler.reset();

        // Mock `isZeroState_` updating value from parent.
        app.isZeroState_ = false;
        await microtasksFinished();

        assertEquals(
            0, mockSearchboxPageHandler.getCallCount('queryAutocomplete'));
      });

  test('inputEnabled attribute reflected on composebox', async () => {
    const contextualComposebox = contextualTasksApp.$.composebox;

    // Default state should be enabled
    assertTrue(contextualComposebox.inputEnabled);
    assertTrue(contextualComposebox.hasAttribute('input-enabled'));

    // Disable input
    contextualComposebox.inputEnabled = false;
    await contextualComposebox.updateComplete;

    assertFalse(contextualComposebox.hasAttribute('input-enabled'));

    // Enable input
    contextualComposebox.inputEnabled = true;
    await contextualComposebox.updateComplete;

    assertTrue(contextualComposebox.hasAttribute('input-enabled'));
  });

  test('lens overlay showing updates placeholder', async () => {
    const contextualComposebox = contextualTasksApp.$.composebox;
    const innerComposebox = contextualComposebox.$.composebox;
    const inputElement = innerComposebox.$.input;

    // Initially false, placeholder override should be empty.
    assertFalse(contextualComposebox.maybeShowOverlayHintText);
    await contextualComposebox.updateComplete;
    await innerComposebox.updateComplete;
    assertEquals('', innerComposebox.inputPlaceholderOverride);

    const initialPlaceholder = inputElement.placeholder;

    // Set to true.
    contextualComposebox.maybeShowOverlayHintText = true;
    await contextualComposebox.updateComplete;
    await innerComposebox.updateComplete;

    assertTrue(contextualComposebox.maybeShowOverlayHintText);
    assertEquals('Test Lens Hint', innerComposebox.inputPlaceholderOverride);
    assertEquals('Test Lens Hint', inputElement.placeholder);

    // Set back to false.
    contextualComposebox.maybeShowOverlayHintText = false;
    await contextualComposebox.updateComplete;
    await innerComposebox.updateComplete;

    assertFalse(contextualComposebox.maybeShowOverlayHintText);
    assertEquals('', innerComposebox.inputPlaceholderOverride);
    assertEquals(initialPlaceholder, inputElement.placeholder);
  });

  test('SuggestionsHiddenWhenDropdownNotShown', async () => {
    loadTimeData.overrideValues({
      composeboxShowTypedSuggestWithContext: false,
      enableNativeZeroStateSuggestions: true,
    });

    contextualTasksApp.isShownInTab_ = true;
    const contextualComposebox = contextualTasksApp.$.composebox;
    contextualTasksApp.isZeroState_ = true;
    await contextualComposebox.updateComplete;
    await composebox.updateComplete;

    const suggestionsContainer =
        contextualComposebox.$.contextualTasksSuggestionsContainer;
    assertTrue(!!suggestionsContainer, 'Suggestions container should exist');

    // Initial state: No matches yet, so show-dropdown_ should be false.
    assertFalse(
        composebox.hasAttribute('show-dropdown_'),
        'Dropdown should not be shown initially');
    assertEquals(
        'none', getComputedStyle(suggestionsContainer).display,
        'Suggestions should be hidden when dropdown is not shown');

    // Add a file.
    const file = new File(['foo'], 'foo.pdf', {type: 'application/pdf'});
    await uploadFileAndVerify(
        FAKE_TOKEN_STRING, file, composebox, mockSearchboxPageHandler);

    // Provide ZPS matches (empty query).
    await setupAutocompleteResults(
        searchboxCallbackRouterRemote, '', mockTimer);
    await contextualComposebox.updateComplete;
    await composebox.updateComplete;

    // show-dropdown_ should be true now because we have ZPS matches and no
    // input.
    assertTrue(
        composebox.hasAttribute('show-dropdown_'),
        'Dropdown should be shown with ZPS matches after adding a file');

    // The suggestions container should be visible.
    assertNotEquals(
        'none', getComputedStyle(suggestionsContainer).display,
        'Suggestions should be visible when dropdown is shown');

    // Simulate typing.
    const inputElement = composebox.$.input;
    simulateUserInput(inputElement, 'test');

    // Provide typed matches.
    await setupAutocompleteResults(
        searchboxCallbackRouterRemote, 'test', mockTimer);
    await contextualComposebox.updateComplete;
    await composebox.updateComplete;

    // show-dropdown_ should be false because we have a file and
    // composeboxShowTypedSuggestWithContext is false.
    assertFalse(
        composebox.hasAttribute('show-dropdown_'),
        'Dropdown should hide when typing with a file and showTypedSuggestWithContext is false');

    // The CSS rule should hide the suggestions container.
    assertEquals(
        'none', getComputedStyle(suggestionsContainer).display,
        'Suggestions should be hidden via CSS when dropdown is hidden');
  });

  test('Injected input can be added, then deleted from AIM', async () => {
    composebox.injectInput('title', 'thumbnail.jpg', FAKE_TOKEN_STRING);
    await composebox.updateComplete;
    await microtasksFinished();

    // Avoid using $.carousel since may be cached.
    const carousel = composebox.shadowRoot.querySelector('#carousel');
    assertTrue(!!carousel, 'Carousel should be in the DOM');
    const files = carousel.files;
    assertEquals(1, files.length);

    composebox.deleteFile(FAKE_TOKEN_STRING);
    await composebox.updateComplete;
    await microtasksFinished();
    assertFalse(
        !!composebox.shadowRoot.querySelector('#carousel'),
        'Carousel should be removed from the DOM');
  });

  test(
      'Injected input with icon can be added, then deleted from AIM',
      async () => {
        composebox.injectInput('title', '', FAKE_TOKEN_STRING, 'quoteFilled');
        await composebox.updateComplete;
        await microtasksFinished();

        // Avoid using $.carousel since may be cached.
        const carousel = composebox.shadowRoot.querySelector('#carousel');
        assertTrue(!!carousel, 'Carousel should be in the DOM');
        const files = carousel.files;
        assertEquals(1, files.length);

        composebox.deleteFile(FAKE_TOKEN_STRING);
        await composebox.updateComplete;
        await microtasksFinished();
        assertFalse(
            !!composebox.shadowRoot.querySelector('#carousel'),
            'Carousel should be removed from the DOM');
      });

  test(
      'Injected input can be added, then deleted from composebox', async () => {
        composebox.injectInput('title', 'thumbnail.jpg', FAKE_TOKEN_STRING);
        await composebox.updateComplete;
        await microtasksFinished();

        // Avoid using $.carousel since may be cached.
        const carousel = composebox.shadowRoot.querySelector('#carousel');
        assertTrue(!!carousel, 'Carousel should be in the DOM');
        const files = carousel.files;
        assertEquals(1, files.length);

        await deleteLastFile();
        await composebox.updateComplete;
        await microtasksFinished();

        assertFalse(
            !!composebox.shadowRoot.querySelector('#carousel'),
            'Carousel should be removed from the DOM');
      });

  // Test that the Tab key correctly synchronizes the selected index.
  test('TabFocusSyncsSelectedIndex', async () => {
    const contextualComposebox = contextualTasksApp.$.composebox;
    const dropdown =
        (contextualComposebox as any).$.contextualTasksSuggestionsContainer;

    // Simulate focus moving to the first match (index 0) via Tab key.
    dropdown.dispatchEvent(new CustomEvent('match-focusin', {
      detail: {index: 0},
      bubbles: true,
      composed: true,
    }));

    await microtasksFinished();

    // Verify the index is synced in both the parent and the dropdown.
    assertEquals(0, (contextualComposebox as any).selectedMatchIndex_);
    assertEquals(0, dropdown.selectedMatchIndex);
  });

  test('TabFocusPopulatesTextAndEnterSubmits', async () => {
    const contextualComposebox = contextualTasksApp.$.composebox;
    const dropdown =
        (contextualComposebox as any).$.contextualTasksSuggestionsContainer;
    const innerComposebox = (contextualComposebox as any).$.composebox;

    // Setup mock zero-state results.
    const matches = [
      createAutocompleteMatch(
          {contents: 'focus match', destinationUrl: 'https://test.com'}),
    ];
    (contextualComposebox as any).zeroStateSuggestions_ =
        createAutocompleteResultForTesting({
          input: '',
          matches: matches,
        });

    await microtasksFinished();
    await contextualComposebox.updateComplete;

    // Simulate Tab focus (match-focusin).
    dropdown.dispatchEvent(new CustomEvent('match-focusin', {
      detail: {index: 0},
      bubbles: true,
      composed: true,
    }));

    await microtasksFinished();
    await innerComposebox.updateComplete;

    // Simulate pressing Enter to submit.
    dropdown.dispatchEvent(new KeyboardEvent('keydown', {
      key: 'Enter',
      bubbles: true,
      composed: true,
    }));

    // Verify the Mojo handler was called correctly.
    const [index, url] =
        await mockSearchboxPageHandler.whenCalled('openAutocompleteMatch');
    assertEquals(0, index);
    assertEquals('https://test.com', url);

    // After submission, verify the input is cleared by your component logic.
    await microtasksFinished();
    await innerComposebox.updateComplete;
    assertEquals('', innerComposebox.getInputText());
  });
});
