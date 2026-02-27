// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// For composebox tests related to tools, secondary inputs (voice, drag/drop).
import 'chrome://contextual-tasks/app.js';

import type {ContextualTasksAppElement} from 'chrome://contextual-tasks/app.js';
import {BrowserProxyImpl} from 'chrome://contextual-tasks/contextual_tasks_browser_proxy.js';
import type {ComposeboxFile} from 'chrome://resources/cr_components/composebox/common.js';
import type {ComposeboxElement} from 'chrome://resources/cr_components/composebox/composebox.js';
import {PageCallbackRouter as ComposeboxPageCallbackRouter, PageHandlerRemote as ComposeboxPageHandlerRemote} from 'chrome://resources/cr_components/composebox/composebox.mojom-webui.js';
import {ComposeboxProxyImpl} from 'chrome://resources/cr_components/composebox/composebox_proxy.js';
import {ToolMode as ComposeboxToolMode} from 'chrome://resources/cr_components/composebox/composebox_query.mojom-webui.js';
import type {ComposeboxFileCarouselElement} from 'chrome://resources/cr_components/composebox/file_carousel.js';
import {WindowProxy} from 'chrome://resources/cr_components/composebox/window_proxy.js';
import {GlowAnimationState} from 'chrome://resources/cr_components/search/constants.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote, type PageRemote as SearchboxPageRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import {MockTimer} from 'chrome://webui-test/mock_timer.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestContextualTasksBrowserProxy} from './test_contextual_tasks_browser_proxy.js';
import {assertStyle, installMock, mockInputState} from './test_utils.js';

const ADD_FILE_CONTEXT_FN = 'addFileContext';

type MockContextualTasksAppElement =
    Omit<ContextualTasksAppElement,|'isZeroState_'|'isShownInTab_'>&{
      isZeroState_: boolean,
      isShownInTab_: boolean,
    };

// Checks if suggestions container is rendered yet.
function checkIfCanFindSuggestionsContainer(
    contextualTasksApp: MockContextualTasksAppElement, canFind: boolean) {
  const suggestions = contextualTasksApp.$.composebox.shadowRoot.querySelector(
      '#contextualTasksSuggestionsContainer');

  if (canFind) {
    assertTrue(!!suggestions, 'Suggestions container should be present in DOM');
  } else {
    assertEquals(
        null, suggestions, 'Suggestions container should not be in DOM');
  }
}

async function dispatchDragAndDropEvent(dropZone: Element, files: File[]) {
  if (!dropZone) {
    throw new Error(
        'dispatchDragAndDropEvent: #composebox drop zone not rendered.');
  }

  const enterEvent = createDragEvent('dragenter', files);
  dropZone.dispatchEvent(enterEvent);
  await microtasksFinished();

  const overEvent = createDragEvent('dragover', files);
  dropZone.dispatchEvent(overEvent);
  await microtasksFinished();

  const dropEvent = createDragEvent('drop', files);
  dropZone.dispatchEvent(dropEvent);
  await microtasksFinished();
}

// Creates drag event that is compatible across all OS's + w/bots
function createDragEvent(type: string, files: File[]): DragEvent {
  const event = new DragEvent(type, {
    bubbles: true,
    cancelable: true,
    composed: true,
  });

  const mockDataTransfer = {
    files: files,
    types: ['Files', 'text/plain'],
    items: files.map(f => ({
                       kind: 'file',
                       type: f.type,
                       getAsFile: () => f,
                     })),
    effectAllowed: 'all',
    dropEffect: 'copy',
    getData: () => '',
  };

  Object.defineProperty(event, 'dataTransfer', {
    value: mockDataTransfer,
  });
  return event;
}

class MockSpeechRecognition {
  voiceSearchInProgress: boolean = false;
  onresult:
      ((this: MockSpeechRecognition,
        ev: SpeechRecognitionEvent) => void)|null = null;
  onend: (() => void)|null = null;
  onerror:
      ((this: MockSpeechRecognition,
        ev: SpeechRecognitionErrorEvent) => void)|null = null;
  interimResults = true;
  continuous = false;
  constructor() {
    mockSpeechRecognition = this;
  }
  start() {
    this.voiceSearchInProgress = true;
  }
  stop() {
    this.voiceSearchInProgress = false;
  }
  abort() {
    this.voiceSearchInProgress = false;
    if (this.onend) {
      this.onend();
    }
  }
}

let mockSpeechRecognition: MockSpeechRecognition;

function createResults(n: number): globalThis.SpeechRecognitionEvent {
  return {
    results: Array.from(Array(n)).map(() => {
      return {
        isFinal: false,
        0: {
          transcript: 'foo',
          confidence: 1,
        },
      } as unknown as SpeechRecognitionResult;
    }),
    resultIndex: 0,
  } as unknown as SpeechRecognitionEvent;
}

function getVoiceSearchButton(composeboxElement: ComposeboxElement):
    HTMLElement|null {
  return composeboxElement.shadowRoot.querySelector<HTMLElement>(
      '#voiceSearchButton');
}

function disableAnimationsRecursively(element: Element) {
  const noAnimation = document.createElement('style');
  noAnimation.textContent = `
    :host, * {
    transition: none !important;
    animation: none !important;
    transition-duration: 0s !important;
    animation-duration: 0s !important;
    }
  `;

  if (element.shadowRoot) {
    element.shadowRoot.appendChild(noAnimation);

    const children = element.shadowRoot.querySelectorAll('*');
    children.forEach(child => disableAnimationsRecursively(child));
  }
}

suite('ContextualTasksComposeboxMiscInputsTest', () => {
  let contextualTasksApp: MockContextualTasksAppElement;
  let composebox: any;
  let testProxy: TestContextualTasksBrowserProxy;
  let mockComposeboxPageHandler: TestMock<ComposeboxPageHandlerRemote>;
  let mockSearchboxPageHandler: TestMock<SearchboxPageHandlerRemote>;
  let searchboxCallbackRouterRemote: SearchboxPageRemote;
  let windowProxy: TestMock<WindowProxy>;
  let mockTimer: MockTimer;
  let metrics: MetricsTracker;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    mockTimer = new MockTimer();

    metrics = fakeMetricsPrivate();

    loadTimeData.overrideValues({
      contextualMenuUsePecApi: false,
      composeboxShowTypedSuggest: true,
      composeboxShowZps: true,
      enableBasicModeZOrder: true,
      composeboxShowContextMenu: true,
    });

    testProxy = new TestContextualTasksBrowserProxy('https://google.com');
    BrowserProxyImpl.setInstance(testProxy);

    mockComposeboxPageHandler = TestMock.fromClass(ComposeboxPageHandlerRemote);
    mockSearchboxPageHandler = TestMock.fromClass(SearchboxPageHandlerRemote);

    const searchboxCallbackRouter = new SearchboxPageCallbackRouter();
    searchboxCallbackRouterRemote =
        searchboxCallbackRouter.$.bindNewPipeAndPassRemote();
    ComposeboxProxyImpl.setInstance(new ComposeboxProxyImpl(
        mockComposeboxPageHandler as any, new ComposeboxPageCallbackRouter(),
        mockSearchboxPageHandler as any, searchboxCallbackRouter));

    contextualTasksApp = document.createElement('contextual-tasks-app') as
        unknown as MockContextualTasksAppElement;
    await customElements.whenDefined('contextual-tasks-app');
    document.body.appendChild(contextualTasksApp);

    await microtasksFinished();

    disableAnimationsRecursively(contextualTasksApp);

    composebox = contextualTasksApp.$.composebox.$.composebox;

    windowProxy = installMock(WindowProxy);
    windowProxy.setResultFor('setTimeout', 0);
    window.webkitSpeechRecognition =
        MockSpeechRecognition as unknown as typeof SpeechRecognition;

    searchboxCallbackRouterRemote.onInputStateChanged(mockInputState);
    await microtasksFinished();
  });

  teardown(() => {
    mockTimer.uninstall();
  });

  test('sets is-dragging-file attribute on dragenter', async () => {
    // Get composebox div in cr-composebox
    const dropZone = composebox.$.composebox;

    assertFalse(composebox.hasAttribute('is-dragging-file'));

    dropZone.dispatchEvent(new DragEvent('dragenter', {
      bubbles: true,
      composed: true,
    }));
    await microtasksFinished();

    assertTrue(composebox.hasAttribute('is-dragging-file'));
    assertEquals(GlowAnimationState.DRAGGING, composebox.animationState);
  });

  test('removes is-dragging-file attribute on dragleave', async () => {
    const dropZone = composebox.$.composebox;

    composebox.animationState = GlowAnimationState.DRAGGING;
    dropZone.dispatchEvent(new DragEvent('dragenter', {
      bubbles: true,
      composed: true,
    }));
    dropZone.dispatchEvent(new DragEvent('dragleave', {
      bubbles: true,
      composed: true,
    }));
    await microtasksFinished();

    assertFalse(composebox.hasAttribute('is-dragging-file'));
    assertEquals(GlowAnimationState.NONE, composebox.animationState);
  });

  test('accepts a dropped file and adds it to the carousel', async () => {
    const dropZone = composebox.$.composebox;
    // Same token for auto inject (mac) and manual (linux/windows)
    const sharedToken = '12345678-1234-1234-1234-123456789abc';
    mockSearchboxPageHandler.setResultFor(
        ADD_FILE_CONTEXT_FN, Promise.resolve(sharedToken));

    const file = new File(['content'], 'foo.pdf', {type: 'application/pdf'});
    // Automatically add file (Mac)
    await dispatchDragAndDropEvent(dropZone, [file]);

    await mockSearchboxPageHandler.whenCalled(ADD_FILE_CONTEXT_FN);
    assertEquals(1, mockSearchboxPageHandler.getCallCount(ADD_FILE_CONTEXT_FN));
    assertFalse(composebox.hasAttribute('is-dragging-file'));

    const mockAddedFile: ComposeboxFile = {
      uuid: sharedToken,
      name: 'foo.pdf',
      status: 0,
      type: 'application/pdf',
      isDeletable: true,
      objectUrl: null,
      dataUrl: null,
      url: null,
      tabId: null,
    };
    composebox.addFileContextForTesting(mockAddedFile);
    await microtasksFinished();

    const carousel: ComposeboxFileCarouselElement|null =
        composebox.shadowRoot.querySelector('cr-composebox-file-carousel');

    assertTrue(!!carousel, 'Carousel should render');

    const carouselFiles = carousel.files;
    assertEquals(1, carouselFiles.length);
    assertEquals('foo.pdf', carouselFiles[0]!.name);
  });

  test('does not accept wrong file type', async () => {
    const dropZone = composebox.$.composebox;
    const testFile =
        new File(['foo'], 'malware.exe', {type: 'application/x-msdownload'});
    await dispatchDragAndDropEvent(dropZone, [testFile]);

    assertEquals(0, mockSearchboxPageHandler.getCallCount(ADD_FILE_CONTEXT_FN));
  });

  test('accepts images', async () => {
    const dropZone = composebox.$.composebox;
    const file = new File(['content'], 'foo.png', {type: 'image/png'});
    await dispatchDragAndDropEvent(dropZone, [file]);
    await mockSearchboxPageHandler.whenCalled(ADD_FILE_CONTEXT_FN);
    assertEquals(1, mockSearchboxPageHandler.getCallCount(ADD_FILE_CONTEXT_FN));
  });

  test('ExpandAnimationState', async function() {
    contextualTasksApp.$.composebox.startExpandAnimation();
    await microtasksFinished();
    assertEquals('expanding', composebox.animationState);
  });

  test('voice search starts as hidden', async () => {
    const composebox = contextualTasksApp.$.composebox.$.composebox;
    const voiceSearchElement = (composebox as any).$.voiceSearch;
    await microtasksFinished();
    assertStyle(voiceSearchElement, 'display', 'none');
  });

  test(
      'clicking voice search starts speech recognition, hides the composebox',
      async () => {
        const composeboxDiv =
            contextualTasksApp.$.composebox.$.composebox.$.composebox;
        const composebox = contextualTasksApp.$.composebox.$.composebox;
        const voiceSearchButton = getVoiceSearchButton(composebox);
        voiceSearchButton!.click();
        await microtasksFinished();

        assertTrue(mockSpeechRecognition.voiceSearchInProgress);
        assertStyle(composeboxDiv, 'opacity', '0');
        assertStyle(composebox.$.voiceSearch, 'display', 'inline');
        assertEquals(composebox.animationState, GlowAnimationState.LISTENING);
        assertEquals(
            1,
            metrics.count(
                'ContextualTasks.VoiceSearch.State',
                /* VOICE_SEARCH_BUTTON_CLICKED */ 0),
            'Voice search button clicked metric count is incorrect');
      });

  test(
      'on voice search result updates the searchbox' +
          'input (final, continuous) but no submit',
      async () => {
        composebox.autoSubmitVoiceSearch = false;
        await composebox.updateComplete;
        await microtasksFinished();

        const voiceSearchButton = getVoiceSearchButton(composebox);
        voiceSearchButton!.click();

        await composebox.updateComplete;
        await microtasksFinished();

        assertEquals(
            composebox.animationState, GlowAnimationState.LISTENING,
            'Animation state should be LISTENING');
        assertTrue(
            composebox.inVoiceSearchMode_,
            'Should be in voice search mode after clicking button');

        assertEquals(
            1,
            metrics.count(
                'ContextualTasks.VoiceSearch.State',
                /* VOICE_SEARCH_BUTTON_CLICKED */ 0),
            'Voice search button clicked metric count is incorrect');
        assertTrue(
            composebox.inVoiceSearchMode_,
            'Should be in voice search mode after clicking button');

        const result = createResults(2);
        Object.assign(result.results[0]![0]!, {transcript: 'hello'});
        Object.assign(result.results[1]![0]!, {transcript: 'world'});

        mockSpeechRecognition.onresult!(result);

        const voiceSearchElement = composebox.$.voiceSearch;
        const voiceSearchInput = voiceSearchElement.$.input;

        await microtasksFinished();
        await composebox.updateComplete;
        await voiceSearchElement.updateComplete;
        await voiceSearchInput.updateComplete;

        assertEquals(
            'helloworld', voiceSearchInput.value,
            'Input should be updated immediately on result');
        assertEquals(
            'helloworld', composebox.transcript_,
            'Composebox transcript should be updated with voice result');
        assertEquals(
            'helloworld', voiceSearchElement.transcript_,
            'Voice search transcript should be updated with voice result');

        assertEquals(
            '', composebox.input_,
            'Composebox input should be empty if not final result');

        assertEquals(
            0,
            metrics.count(
                'ContextualTasks.VoiceSearch.State',
                /* VOICE_SEARCH_TRANSCRIPTION_SUCCESS */ 2),
            'Voice search transcription success\
                metric count is incorrect for helloworld');

        const result2 = createResults(2);
        Object.assign(result2.results[0]![0]!, {transcript: 'hello'});
        Object.assign(result2.results[1]![0]!, {transcript: 'hellogoodbye'});
        /* Done with transcribing once there is one `isFinal`.
         * This is because it is in continuous mode. Means terminate and
         * take the specific result marked with `resultIndex`.
         * Only 'hellogoodbye' should be taken as final result given
         * we set its flag 'isFinal' to true. There
         * can only be one final result.
         */
        Object.assign(result2.results[1]!, {isFinal: true});
        (result2 as any).resultIndex = 1;
        mockSpeechRecognition.onresult!(result2);

        await microtasksFinished();
        await composebox.updateComplete;


        assertEquals(
            'hellogoodbye', composebox.input_,
            'Composebox input should be updated with final result');

        assertEquals(
            '', voiceSearchElement.transcript_,
            'Voice search transcript should be cleared with final result');
        assertEquals(
            '', voiceSearchInput.value,
            'Voice search input value should be cleared with final result');

        assertEquals(
            1,
            metrics.count(
                'ContextualTasks.VoiceSearch.State',
                /* VOICE_SEARCH_TRANSCRIPTION_SUCCESS */ 1),
            'Voice transcription success metric count is wrong: hellogoodbye');

        assertNotEquals(
            composebox.animationState, GlowAnimationState.SUBMITTING,
            'Query is not submitted via submitQuery_()');
        assertFalse(
            composebox.inVoiceSearchMode_,
            'Should exit voice search mode after submit');
        assertEquals(
            composebox.transcript_, '',
            'Composebox transcript should be cleared after voice mode end');
      });

  test('on voice search result submits if auto submit enabled', async () => {
    composebox.autoSubmitVoiceSearch = true;

    const voiceSearchButton = getVoiceSearchButton(composebox);
    voiceSearchButton!.click();
    await composebox.updateComplete;
    await microtasksFinished();

    assertTrue(
        composebox.inVoiceSearchMode_,
        'Should be in voice search mode after clicking button');
    assertEquals(
        composebox.animationState, GlowAnimationState.LISTENING,
        'Animation state should be LISTENING');
    assertEquals(
        1,
        metrics.count(
            'ContextualTasks.VoiceSearch.State',
            /* VOICE_SEARCH_BUTTON_CLICKED */ 0),
        'Voice search button clicked metric count is incorrect');
    const [callback] = await windowProxy.whenCalled('setTimeout');

    const result = createResults(2);
    Object.assign(result.results[0]![0]!, {transcript: 'hello'});
    Object.assign(result.results[1]![0]!, {transcript: 'world2'});

    const voiceSearchElement = composebox.$.voiceSearch;
    const voiceSearchInput = voiceSearchElement.$.input;

    mockSpeechRecognition.onresult!(result);

    await microtasksFinished();
    await composebox.updateComplete;
    await voiceSearchElement.updateComplete;
    await voiceSearchInput.updateComplete;

    assertEquals('helloworld2', voiceSearchInput.value);
    assertEquals(
        'helloworld2', voiceSearchElement.transcript_,
        'Voice search transcript should be updated immediately on result');
    assertEquals(
        'helloworld2', composebox.transcript_,
        'Transcript should be updated immediately on result');

    assertEquals(
        '', composebox.input_,
        'Input should not be updated in composebox without final result');

    callback();
    await microtasksFinished();
    await composebox.updateComplete;

    assertEquals(
        1,
        metrics.count(
            'ContextualTasks.VoiceSearch.State',
            /* VOICE_SEARCH_TRANSCRIPTION_SUCCESS */ 1),
        'Voice transcription success metric count is wrong: helloworld2');
    assertEquals(
        composebox.animationState, GlowAnimationState.SUBMITTING,
        'Query is submitted via submitQuery_()');
    assertEquals(composebox.input_, '', 'Input should be cleared after submit');

    assertEquals(
        '', voiceSearchInput.value,
        'Voice Search input should be cleared after submit');
    assertEquals(
        '', voiceSearchElement.transcript_,
        'Voice Search transcript should be cleared after submit');

    assertFalse(
        composebox.inVoiceSearchMode_,
        'Should exit voice search mode after submit');
    assertEquals(
        composebox.transcript_, '',
        'Composebox transcript should be cleared after submit');
  });

  test('on error shows error container for NOT_ALLOWED', async () => {
    const composeboxDiv =
        contextualTasksApp.$.composebox.$.composebox.$.composebox;
    const composebox = contextualTasksApp.$.composebox.$.composebox;
    const voiceSearchButton = getVoiceSearchButton(composebox);
    voiceSearchButton!.click();
    await microtasksFinished();

    mockSpeechRecognition.onerror!
        ({error: 'not-allowed'} as SpeechRecognitionErrorEvent);
    await microtasksFinished();
    await composebox.updateComplete;

    const voiceSearchElement = composebox.$.voiceSearch;
    const errorContainer =
        voiceSearchElement.shadowRoot.querySelector<HTMLElement>(
            '#error-container');
    const inputElement =
        voiceSearchElement.shadowRoot.querySelector<HTMLTextAreaElement>(
            '#input');

    assertTrue(!!errorContainer);
    assertFalse(errorContainer.hidden);
    assertFalse(errorContainer.hidden, 'Error container should not be hidden');
    assertTrue(inputElement!.hidden);
    assertStyle(composeboxDiv, 'opacity', '0');
    assertStyle(composebox.$.voiceSearch, 'display', 'inline');
    assertEquals(composebox.animationState, GlowAnimationState.LISTENING);

    mockSpeechRecognition.onend!();
    assertEquals(
        1,
        metrics.count(
            'ContextualTasks.VoiceSearch.State',
            /* VOICE_SEARCH_ERROR */ 2),
        'Voice search error metric count is incorrect');
  });

  test(
      'on voice search error does not show non-NOT-ALLOWED errors, \
      and then hides overlay',
      async () => {
        const composeboxDiv =
            contextualTasksApp.$.composebox.$.composebox.$.composebox;
        const composebox = contextualTasksApp.$.composebox.$.composebox;
        composebox.$.voiceSearch.start();
        await microtasksFinished();
        assertEquals(
            0,
            metrics.count(
                'ContextualTasks.VoiceSearch.State',
                /* VOICE_SEARCH_BUTTON_CLICKED */ 0),
            'Voice search button clicked metric count is incorrect');

        mockSpeechRecognition.onerror!
            ({error: 'network'} as SpeechRecognitionErrorEvent);
        await composebox.updateComplete;
        await microtasksFinished();

        const voiceSearchElement = composebox.$.voiceSearch;
        const errorContainer =
            voiceSearchElement.shadowRoot.querySelector<HTMLElement>(
                '#error-container');
        assertTrue(!!errorContainer);
        assertTrue(errorContainer.hidden);

        // Flush the macrotask queue / event loop
        await new Promise(resolve => setTimeout(resolve, 0));

        await microtasksFinished();

        assertStyle(voiceSearchElement, 'display', 'none');
        assertStyle(composeboxDiv, 'display', 'flex');

        mockSpeechRecognition.onend!();
        assertEquals(
            1,
            metrics.count(
                'ContextualTasks.VoiceSearch.State',
                /* VOICE_SEARCH_ERROR_AND_CANCELED */ 3),
            'Voice search error-canceled metric count is incorrect');
      });

  test('clicking cancel button cancels voice search', async () => {
    const composeboxDiv =
        contextualTasksApp.$.composebox.$.composebox.$.composebox;
    const composebox = contextualTasksApp.$.composebox.$.composebox;
    const voiceSearchButton = getVoiceSearchButton(composebox);
    const voiceSearchElement = composebox.$.voiceSearch;
    voiceSearchButton!.click();
    await microtasksFinished();

    const result = createResults(2);
    Object.assign(result.results[0]![0]!, {transcript: 'hello'});
    Object.assign(result.results[1]![0]!, {transcript: 'world'});
    mockSpeechRecognition.onresult!(result);
    await microtasksFinished();

    const voiceSearchInput = voiceSearchElement.$.input;

    assertEquals('helloworld', voiceSearchInput.value);

    voiceSearchElement.$.closeButton.click();

    await microtasksFinished();

    assertStyle(
        voiceSearchElement, 'display', 'none', 'Voice search should be hidden');
    assertStyle(composeboxDiv, 'display', 'flex', 'Composebox should be shown');
    assertEquals(composebox.$.input.value, '', 'Input should be cleared');

    assertEquals(
        1,
        metrics.count(
            'ContextualTasks.VoiceSearch.State',
            /* VOICE_SEARCH_USER_CANCELED*/ 4),
        'Voice search canceled metric count is incorrect');
  });

  test(
      'suggestions show correctly in side panel zero state based on loading',
      async () => {
        contextualTasksApp.isShownInTab_ = false;
        contextualTasksApp.isZeroState_ = true;

        contextualTasksApp.setEnableNativeZeroStateSuggestionsForTesting(false);

        await contextualTasksApp.updateComplete;
        await contextualTasksApp.$.composebox.updateComplete;
        await microtasksFinished();

        assertStyle(contextualTasksApp.$.composebox, 'bottom', '0px');

        checkIfCanFindSuggestionsContainer(
            contextualTasksApp, /*canFind=*/ false);

        contextualTasksApp.setEnableNativeZeroStateSuggestionsForTesting(true);

        await contextualTasksApp.updateComplete;
        await contextualTasksApp.$.composebox.updateComplete;
        await microtasksFinished();

        assertStyle(contextualTasksApp.$.composebox, 'bottom', '30px');

        (contextualTasksApp.$.composebox as any).isLoading_ = true;
        await contextualTasksApp.updateComplete;
        await contextualTasksApp.$.composebox.updateComplete;
        await microtasksFinished();
        checkIfCanFindSuggestionsContainer(
            contextualTasksApp, /*canFind=*/ true);
        const firstMatch: any =
            contextualTasksApp.$.composebox.$
                .contextualTasksSuggestionsContainer.shadowRoot.querySelector(
                    '#match0');
        assertTrue(
            !!firstMatch.$.textContainer,
            'First suggestion match should exist');
        assertStyle(
            firstMatch.$.textContainer, 'animation-duration', '2s',
            'When in loading side panel but in zero-state, suggestions\
                should be in loading state');

        (contextualTasksApp.$.composebox as any).isLoading_ = false;
        await contextualTasksApp.updateComplete;
        await contextualTasksApp.$.composebox.updateComplete;
        await contextualTasksApp.$.composebox.$
            .contextualTasksSuggestionsContainer.updateComplete;
        await firstMatch.updateComplete;
        await microtasksFinished();

        assertStyle(
            firstMatch.$.textContainer, 'animation', 'none',
            'When not in loading side panel zero-state,\
                suggestions should be normal');

        contextualTasksApp.isZeroState_ = false;
        await contextualTasksApp.updateComplete;
        await contextualTasksApp.$.composebox.updateComplete;
        await microtasksFinished();
        assertTrue(
            contextualTasksApp.$.composebox.$
                .contextualTasksSuggestionsContainer.hidden,
            'Dropdown should be hidden when NOT in zero state');
      });

  test(
      'suggestions show correctly in full tab zero state based on loading',
      async () => {
        contextualTasksApp.isShownInTab_ = true;
        contextualTasksApp.isZeroState_ = true;

        contextualTasksApp.setEnableNativeZeroStateSuggestionsForTesting(false);

        await contextualTasksApp.updateComplete;
        await contextualTasksApp.$.composebox.updateComplete;
        await microtasksFinished();

        checkIfCanFindSuggestionsContainer(
            contextualTasksApp, /*canFind=*/ false);

        contextualTasksApp.setEnableNativeZeroStateSuggestionsForTesting(true);


        await contextualTasksApp.updateComplete;
        await contextualTasksApp.$.composebox.updateComplete;
        await microtasksFinished();
        checkIfCanFindSuggestionsContainer(
            contextualTasksApp, /*canFind=*/ true);
        assertStyle(contextualTasksApp.$.flexCenterContainer, 'top', '88px');

        (contextualTasksApp.$.composebox as any).isLoading_ = true;
        await contextualTasksApp.updateComplete;
        await contextualTasksApp.$.composebox.updateComplete;
        await contextualTasksApp.$.composebox.$
            .contextualTasksSuggestionsContainer.updateComplete;
        const firstMatch: any =
            contextualTasksApp.$.composebox.$
                .contextualTasksSuggestionsContainer.shadowRoot.querySelector(
                    '#match0');
        assertTrue(
            !!firstMatch.$.textContainer,
            'First suggestion match should exist');

        await microtasksFinished();

        assertStyle(
            firstMatch.$.textContainer, 'animation-duration', '2s',
            'When in loading full tab zero-state,\
                suggestions should be in loading state');
        (contextualTasksApp.$.composebox as any).isLoading_ = false;
        await contextualTasksApp.updateComplete;
        await contextualTasksApp.$.composebox.updateComplete;
        await contextualTasksApp.$.composebox.$
            .contextualTasksSuggestionsContainer.updateComplete;
        await firstMatch.updateComplete;
        await microtasksFinished();

        assertStyle(
            firstMatch.$.textContainer, 'animation', 'none',
            'When not in loading full tab but in zero-state,\
                suggestions should be normal');

        contextualTasksApp.isZeroState_ = false;

        await contextualTasksApp.updateComplete;
        await contextualTasksApp.$.composebox.updateComplete;
        await contextualTasksApp.$.composebox.$
            .contextualTasksSuggestionsContainer.updateComplete;
        await firstMatch.updateComplete;

        await microtasksFinished();
        assertTrue(
            contextualTasksApp.$.composebox.$
                .contextualTasksSuggestionsContainer.hidden,
            'Dropdown should be hidden when NOT in zero state',
        );
      });

  test('tool click event triggers tool mode change', async () => {
    const contextEntrypoint =
        composebox.shadowRoot.querySelector('#contextEntrypoint');
    assertTrue(!!contextEntrypoint);
    contextEntrypoint.showModelPicker = false;

    await microtasksFinished();
    await composebox.updateComplete;

    await contextEntrypoint.dispatchEvent(
        new CustomEvent('tool-click', {
          detail: {toolMode: ComposeboxToolMode.kDeepSearch},
          bubbles: true,
          composed: true,
        }));
    await microtasksFinished();
    await composebox.updateComplete;
    assertEquals(
        ComposeboxToolMode.kDeepSearch,
        composebox.activeToolMode,
        'Active tool should be Deep Search after clicking tool');
    await contextEntrypoint.dispatchEvent(
        new CustomEvent('tool-click', {
          detail: {toolMode: ComposeboxToolMode.kDeepSearch},
          bubbles: true,
          composed: true,
        }));

    await microtasksFinished();
    await composebox.updateComplete;

    assertEquals(
        ComposeboxToolMode.kUnspecified,
        composebox.activeToolMode,
        'Active tool should be unspecified after clicking tool twice');
  });

  test('tool click event triggers tool mode change', async () => {
    const contextEntrypoint =
        composebox.shadowRoot.querySelector('#contextEntrypoint');
    assertTrue(!!contextEntrypoint);
    contextEntrypoint.showModelPicker = false;

    await microtasksFinished();
    await composebox.updateComplete;

    await contextEntrypoint.dispatchEvent(
        new CustomEvent(
            'create-image-click',
            {
              bubbles: true,
              composed: true,
            },
            ));
    await microtasksFinished();
    await composebox.updateComplete;
    assertEquals(
        ComposeboxToolMode.kImageGen, composebox.activeToolMode,
        'Active tool should be nano after clicking tool');
    await contextEntrypoint.dispatchEvent(
        new CustomEvent(
            'create-image-click',
            {
              bubbles: true,
              composed: true,
            },
            ));

    await microtasksFinished();
    await composebox.updateComplete;

    assertEquals(
        ComposeboxToolMode.kUnspecified,
        composebox.activeToolMode,
        'Active tool should be unspecified after clicking tool twice');
  });

  test('Deepsearch tool is not reset after submitting a query', async () => {
    // To change the carousel's tool selection, must send `tool-click` event to
    // button, but because this test should work in both tool picker mode, and
    // context menu mode, we just call the underlying function that responds to
    // both `tool-click` and individual `deep-search-click` events.
    composebox.onToolClickForTesting(ComposeboxToolMode.kDeepSearch);
    await composebox.updateComplete;
    await microtasksFinished();

    let deepSearchChip = composebox.shadowRoot.querySelector('#deepSearchChip');

    assertTrue(!!deepSearchChip, 'Deep search chip should be present');
    composebox.$.input.value = 'test';
    composebox.$.input.dispatchEvent(new Event('input'));
    // Since we cannot create a fake AutocompleteResult easily (35+ fields),
    // we populate the result in a different way. There is an assert statement
    // in cr-component composebox.ts that checks if AutocompleteResult is
    // present, as it indicates if `input` is present, as well as
    // things like `contextFileSize` being nonzero).
    composebox.contextFilesSize_ = 2;
    await composebox.updateComplete;
    await microtasksFinished();

    composebox.$.submitContainer.click();

    await composebox.updateComplete;
    await microtasksFinished();

    deepSearchChip = composebox.shadowRoot.querySelector('#deepSearchChip');
    assertTrue(
        !!deepSearchChip,
        'Deep search chip not should be hidden' +
            'after submitting');
  });

  test('Image tool is not reset after submitting a query', async () => {
    composebox.onToolClickForTesting(ComposeboxToolMode.kImageGen);
    await composebox.updateComplete;
    await microtasksFinished();

    let imageChip = composebox.shadowRoot.querySelector('#nanoBananaChip');

    assertTrue(!!imageChip, 'Image chip should be present');
    composebox.$.input.value = 'test';
    composebox.$.input.dispatchEvent(new Event('input'));

    // Fake a finished query:
    composebox.contextFilesSize_ = 2;
    await composebox.updateComplete;
    await microtasksFinished();

    composebox.$.submitContainer.click();

    await composebox.updateComplete;
    await microtasksFinished();

    imageChip = composebox.shadowRoot.querySelector('#nanoBananaChip');
    assertTrue(
        !!imageChip,
        'Banana nano chip not should be hidden' +
            'after submitting');
  });

  test('Canvas tool is not reset after submitting a query', async () => {
    composebox.onToolClickForTesting(ComposeboxToolMode.kCanvas);
    await composebox.updateComplete;
    await microtasksFinished();

    let canvasChip = composebox.shadowRoot.querySelector('#canvasChip');

    assertTrue(!!canvasChip, 'Canvas chip should be present');
    composebox.$.input.value = 'test';
    composebox.$.input.dispatchEvent(new Event('input'));

    // Fake a finished query:
    composebox.contextFilesSize_ = 2;
    await composebox.updateComplete;
    await microtasksFinished();

    composebox.$.submitContainer.click();

    await composebox.updateComplete;
    await microtasksFinished();

    canvasChip = composebox.shadowRoot.querySelector('#canvasChip');
    assertTrue(
        !!canvasChip, 'Canvas chip should not be hidden after submitting');
  });

  test('Deepsearch mode: cancel resets mode', async () => {
    composebox.onToolClickForTesting(ComposeboxToolMode.kDeepSearch);

    await composebox.updateComplete;
    await microtasksFinished();

    let deepSearchChip = composebox.shadowRoot.querySelector('#deepSearchChip');

    assertTrue(!!deepSearchChip, 'Deep search chip should be present');
    // Simulate cancel button click without having to fully render button.
    composebox.onCancelClick_();

    await composebox.updateComplete;
    await microtasksFinished();

    deepSearchChip = composebox.shadowRoot.querySelector('#deepSearchChip');
    assertFalse(!!deepSearchChip, 'Deep search chip should be removed');
  });

  test('Image mode: cancel resets mode', async () => {
    composebox.onToolClickForTesting(ComposeboxToolMode.kImageGen);

    await composebox.updateComplete;
    await microtasksFinished();
    let imageChip = composebox.shadowRoot.querySelector('#nanoBananaChip');

    assertTrue(!!imageChip, 'Nano banana chip should be present');
    // Simulate cancel button click without having to fully render button.
    composebox.onCancelClick_();

    await composebox.updateComplete;
    await microtasksFinished();

    imageChip = composebox.shadowRoot.querySelector('#nanoBananaChip');
    assertFalse(!!imageChip, 'Nano banana chip should be removed');
  });

  test('canvas mode: cancel resets mode', async () => {
    composebox.onToolClickForTesting(ComposeboxToolMode.kCanvas);

    await composebox.updateComplete;
    await microtasksFinished();

    let canvasChip = composebox.shadowRoot.querySelector('#canvasChip');

    assertTrue(!!canvasChip, 'Canvas chip should be present');
    // Simulate cancel button click without having to fully render button.
    composebox.onCancelClick_();

    await composebox.updateComplete;
    await microtasksFinished();

    canvasChip = composebox.shadowRoot.querySelector('#canvasChip');
    assertFalse(!!canvasChip, 'Canvas chip should be removed');
  });

  test('Deepsearch mode: esc resets mode', async () => {
    composebox.onToolClickForTesting(ComposeboxToolMode.kDeepSearch);

    await composebox.updateComplete;
    await microtasksFinished();
    let deepSearchChip = composebox.shadowRoot.querySelector('#deepSearchChip');

    assertTrue(!!deepSearchChip, 'Deep search chip should be present');
    composebox.handleEscapeKeyLogic();

    await composebox.updateComplete;
    await microtasksFinished();

    deepSearchChip = composebox.shadowRoot.querySelector('#deepSearchChip');
    assertFalse(!!deepSearchChip, 'Deep search chip should be removed');
  });

  test('Image mode: esc resets mode', async () => {
    composebox.onToolClickForTesting(ComposeboxToolMode.kImageGen);

    await composebox.updateComplete;
    await microtasksFinished();
    let imageChip = composebox.shadowRoot.querySelector('#nanoBananaChip');

    assertTrue(!!imageChip, 'Nano banana chip should be present');
    composebox.handleEscapeKeyLogic();

    await composebox.updateComplete;
    await microtasksFinished();

    imageChip = composebox.shadowRoot.querySelector('#nanoBananaChip');
    assertFalse(!!imageChip, 'Nano banana chip should be removed');
  });

  test('canvas mode: esc resets mode', async () => {
    composebox.onToolClickForTesting(ComposeboxToolMode.kCanvas);

    await composebox.updateComplete;
    await microtasksFinished();
    let canvasChip = composebox.shadowRoot.querySelector('#canvasChip');

    assertTrue(!!canvasChip, 'Canvas chip should be present');
    composebox.handleEscapeKeyLogic();

    await composebox.updateComplete;
    await microtasksFinished();

    canvasChip = composebox.shadowRoot.querySelector('#canvasChip');
    assertFalse(!!canvasChip, 'Canvas chip should be removed');
  });

  test('deep search: thread change resets input', async () => {
    composebox.onToolClickForTesting(ComposeboxToolMode.kDeepSearch);

    await composebox.updateComplete;
    await microtasksFinished();

    let deepSearchChip = composebox.shadowRoot.querySelector('#deepSearchChip');

    assertTrue(!!deepSearchChip, 'Deep search chip should be present');

    testProxy.callbackRouterRemote.onZeroStateChange(/*isZeroState=*/ true);
    await testProxy.callbackRouterRemote.$.flushForTesting();

    await composebox.updateComplete;
    await microtasksFinished();

    deepSearchChip = composebox.shadowRoot.querySelector('#deepSearchChip');
    assertFalse(!!composebox.input_, 'Input value should be cleared');
    assertTrue(
        composebox.fileUploadsComplete, 'File uploads should be complete');
    assertFalse(
        !!composebox.getResultForTesting(),
        'Autocomplete result should be cleared');
  });

  test('image gen: thread change resets input', async () => {
    composebox.onToolClickForTesting(ComposeboxToolMode.kImageGen);

    await composebox.updateComplete;
    await microtasksFinished();

    let imageGenChip = composebox.shadowRoot.querySelector('#nanoBananaChip');

    assertTrue(!!imageGenChip, 'Image gen chip should be present');

    testProxy.callbackRouterRemote.onZeroStateChange(/*isZeroState=*/ true);
    await testProxy.callbackRouterRemote.$.flushForTesting();


    await composebox.updateComplete;
    await microtasksFinished();

    imageGenChip = composebox.shadowRoot.querySelector('#nanoBananaChip');
    assertFalse(!!composebox.input_, 'Input value should be cleared');
    assertTrue(
        composebox.fileUploadsComplete, 'File uploads should be complete');
    assertFalse(
        !!composebox.getResultForTesting(),
        'Autocomplete result should be cleared');
  });

  test('canvas: thread change resets input', async () => {
    composebox.onToolClickForTesting(ComposeboxToolMode.kCanvas);

    await composebox.updateComplete;
    await microtasksFinished();

    let canvasChip = composebox.shadowRoot.querySelector('#canvasChip');

    assertTrue(!!canvasChip, 'Canvas chip should be present');

    testProxy.callbackRouterRemote.onZeroStateChange(/*isZeroState=*/ true);
    await testProxy.callbackRouterRemote.$.flushForTesting();

    await composebox.updateComplete;
    await microtasksFinished();

    canvasChip = composebox.shadowRoot.querySelector('#canvasChip');
    assertFalse(!!composebox.input_, 'Input value should be cleared');
    assertTrue(
        composebox.fileUploadsComplete, 'File uploads should be complete');
    assertFalse(
        !!composebox.getResultForTesting(),
        'Autocomplete result should be cleared');
  });
});
