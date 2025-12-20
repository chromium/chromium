// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/strings.m.js';
import 'chrome://resources/cr_components/composebox/composebox.js';

import type {ComposeboxElement} from 'chrome://resources/cr_components/composebox/composebox.js';
import {VoiceSearchAction} from 'chrome://resources/cr_components/composebox/composebox.js';
import {PageCallbackRouter, PageHandlerRemote} from 'chrome://resources/cr_components/composebox/composebox.mojom-webui.js';
import {ComposeboxProxyImpl} from 'chrome://resources/cr_components/composebox/composebox_proxy.js';
import {WindowProxy} from 'chrome://resources/cr_components/composebox/window_proxy.js';
import {GlowAnimationState} from 'chrome://resources/cr_components/search/constants.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {assertStyle, installMock} from './composebox_test_utils.js';

// Returns a promise that resolves when CSS style has transitioned.
function getTransitionEndPromise(
    element: HTMLElement, property?: string): Promise<void> {
  return new Promise<void>(
      resolve =>
          element.addEventListener('transitionend', (e: TransitionEvent) => {
            if (!property || e.propertyName === property) {
              resolve();
            }
          }));
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
    this.onend!();
  }
}

let mockSpeechRecognition: MockSpeechRecognition;

function createResults(n: number): SpeechRecognitionEvent {
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

suite('Composebox voice search', () => {
  let composeboxElement: ComposeboxElement;
  let handler: TestMock<PageHandlerRemote>;
  let searchboxHandler: TestMock<SearchboxPageHandlerRemote>;
  let windowProxy: TestMock<WindowProxy>;

  suiteSetup(() => {
    loadTimeData.overrideValues({
      expandedComposeboxShowVoiceSearch: true,
      steadyComposeboxShowVoiceSearch: true,
      composeboxShowZps: true,
      composeboxShowTypedSuggest: true,
    });
  });

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = installMock(
        PageHandlerRemote,
        mock => ComposeboxProxyImpl.setInstance(new ComposeboxProxyImpl(
            mock, new PageCallbackRouter(), new SearchboxPageHandlerRemote(),
            new SearchboxPageCallbackRouter())));
    assertTrue(!!handler);
    searchboxHandler = installMock(
        SearchboxPageHandlerRemote,
        mock => ComposeboxProxyImpl.getInstance().searchboxHandler = mock);
    searchboxHandler.setResultFor('getRecentTabs', Promise.resolve({tabs: []}));

    windowProxy = installMock(WindowProxy);
    windowProxy.setResultFor('setTimeout', 0);

    composeboxElement = document.createElement('cr-composebox');
    document.body.appendChild(composeboxElement);
    window.webkitSpeechRecognition =
        MockSpeechRecognition as unknown as typeof SpeechRecognition;
  });

  function getVoiceSearchButton(composeboxElement: ComposeboxElement):
      HTMLElement|null {
    const contextElement = composeboxElement.$.context;
    return contextElement.shadowRoot.querySelector<HTMLElement>(
        '#voiceSearchButton');
  }

  test('voice search button does not show when disabled', async () => {
    loadTimeData.overrideValues({
      steadyComposeboxShowVoiceSearch: false,
      expandedComposeboxShowVoiceSearch: false,
    });
    // Create element again with new loadTimeData values.
    composeboxElement = document.createElement('cr-composebox');
    document.body.appendChild(composeboxElement);
    await microtasksFinished();

    const voiceSearchButton = await getVoiceSearchButton(composeboxElement);
    assertFalse(!!voiceSearchButton);

    // Restore.
    loadTimeData.overrideValues({
      steadyComposeboxShowVoiceSearch: true,
      expandedComposeboxShowVoiceSearch: true,
    });
  });

  test('voice search button shows when enabled', () => {
    const voiceSearchButton = getVoiceSearchButton(composeboxElement);
    assertTrue(!!voiceSearchButton);
  });

  test(
      'clicking voice search starts speech recognition and hides the composebox',
      async () => {
        const hidePromise =
            getTransitionEndPromise(composeboxElement.$.composebox, 'opacity');
        const voiceSearchButton = getVoiceSearchButton(composeboxElement);
        voiceSearchButton!.click();
        await microtasksFinished();
        await hidePromise;

        // Clicking the voice search button should start speech recognition.
        assertTrue(mockSpeechRecognition.voiceSearchInProgress);
        assertStyle(composeboxElement.$.composebox, 'opacity', '0');
        assertStyle(composeboxElement.$.voiceSearch, 'display', 'inline');
        assertEquals(
            composeboxElement.animationState, GlowAnimationState.LISTENING);
      });

  test('on result updates the searchbox input', async () => {
    const result = createResults(2);
    Object.assign(result.results[0]![0]!, {transcript: 'hello'});
    Object.assign(result.results[1]![0]!, {transcript: 'world'});

    // Act.
    mockSpeechRecognition.onresult!(result);
    await microtasksFinished();

    const voiceSearchInput = composeboxElement.$.voiceSearch.$.input;

    assertEquals('helloworld', voiceSearchInput.value);

    // Reset the composebox input.
    voiceSearchInput.value = 'test';
    voiceSearchInput.dispatchEvent(new Event('input'));
    assertEquals('test', voiceSearchInput.value);
    await microtasksFinished();

    const result2 = createResults(2);
    Object.assign(result2.results[0]![0]!, {transcript: 'hello'});
    Object.assign(result2.results[1]![0]!, {transcript: 'goodbye'});

    // Act.
    mockSpeechRecognition.onresult!(result2);
    await microtasksFinished();

    // Speech recognition overrides existing composebox input.
    assertEquals('hellogoodbye', voiceSearchInput.value);
  });

  test('on result submits a query if marked as final', async () => {
    const result = createResults(2);
    Object.assign(result.results[0]!, {isFinal: true});
    Object.assign(result.results[0]![0]!, {transcript: 'hello world'});

    const finalResultEventPromise = new Promise((resolve) => {
      composeboxElement.addEventListener('voice-search-action', (e: Event) => {
        const customEvent = e as CustomEvent<{value: VoiceSearchAction}>;
        if (customEvent.detail.value === VoiceSearchAction.QUERY_SUBMITTED) {
          resolve(customEvent.detail.value);
        }
      }, {once: true});
    });
    const showPromise =
        getTransitionEndPromise(composeboxElement.$.composebox, 'opacity');

    // Act.
    mockSpeechRecognition.onresult!(result);
    await microtasksFinished();
    const finalResult = await finalResultEventPromise;
    assertEquals(VoiceSearchAction.QUERY_SUBMITTED, finalResult);
    await showPromise;

    // The composebox should navigate with the text after `onEnd` is called.
    assertEquals(searchboxHandler.getCallCount('openAutocompleteMatch'), 0);
    assertEquals(searchboxHandler.getCallCount('submitQuery'), 1);

    assertStyle(composeboxElement.$.composebox, 'display', 'flex');
    assertStyle(composeboxElement.$.voiceSearch, 'display', 'none');
  });

  test('idle timer exits voice search if no final result', async () => {
    const voiceSearchButton = getVoiceSearchButton(composeboxElement);
    voiceSearchButton!.click();
    await microtasksFinished();

    assertTrue(mockSpeechRecognition.voiceSearchInProgress);
    const showPromise =
        getTransitionEndPromise(composeboxElement.$.composebox, 'opacity');

    const [callback] = await windowProxy.whenCalled('setTimeout');
    callback();
    await microtasksFinished();
    await showPromise;

    // Assert.
    assertFalse(mockSpeechRecognition.voiceSearchInProgress);
    assertEquals(searchboxHandler.getCallCount('submitQuery'), 0);
    assertStyle(composeboxElement.$.composebox, 'display', 'flex');
    assertStyle(composeboxElement.$.voiceSearch, 'display', 'none');
    assertEquals(composeboxElement.animationState, GlowAnimationState.NONE);
  });

  test('idle timer submits voice search if final result exists', async () => {
    const voiceSearchButton = getVoiceSearchButton(composeboxElement);
    voiceSearchButton!.click();
    await microtasksFinished();

    assertTrue(mockSpeechRecognition.voiceSearchInProgress);

    const result = createResults(2);
    Object.assign(result.results[0]![0]!, {transcript: 'hello'});
    Object.assign(result.results[1]![0]!, {transcript: 'world'});
    const showPromise =
        getTransitionEndPromise(composeboxElement.$.composebox, 'opacity');
    // Act.
    mockSpeechRecognition.onresult!(result);

    const [callback] = await windowProxy.whenCalled('setTimeout');
    callback();
    await microtasksFinished();

    // Assert.
    assertEquals(searchboxHandler.getCallCount('submitQuery'), 1);

    await showPromise;
    assertStyle(composeboxElement.$.composebox, 'display', 'flex');
    assertStyle(composeboxElement.$.voiceSearch, 'display', 'none');
  });

  test('idle timeout with no final result does not submit query', async () => {
    const voiceSearchButton = getVoiceSearchButton(composeboxElement);
    voiceSearchButton!.click();
    await microtasksFinished();

    assertTrue(mockSpeechRecognition.voiceSearchInProgress);

    const result = createResults(2);
    Object.assign(result.results[0]![0]!, {confidence: 0, transcript: 'hello'});
    Object.assign(result.results[1]![0]!, {confidence: 0, transcript: 'world'});

    const showPromise =
        getTransitionEndPromise(composeboxElement.$.composebox, 'opacity');

    // Act.
    mockSpeechRecognition.onresult!(result);

    const [callback] = await windowProxy.whenCalled('setTimeout');
    callback();
    await microtasksFinished();

    // Assert.
    assertEquals(searchboxHandler.getCallCount('submitQuery'), 0);

    await showPromise;
    assertStyle(composeboxElement.$.composebox, 'display', 'flex');
    assertStyle(composeboxElement.$.voiceSearch, 'display', 'none');
  });


  test(
      'on end exits voice search if no final result is available', async () => {
        const voiceSearchButton = getVoiceSearchButton(composeboxElement);
        voiceSearchButton!.click();
        await microtasksFinished();

        assertTrue(mockSpeechRecognition.voiceSearchInProgress);

        const result = createResults(2);
        Object.assign(
            result.results[0]![0]!, {confidence: 0, transcript: 'hello'});
        Object.assign(
            result.results[1]![0]!, {confidence: 0, transcript: 'world'});

        const showPromise =
            getTransitionEndPromise(composeboxElement.$.composebox, 'opacity');

        // Act.
        mockSpeechRecognition.onresult!(result);
        mockSpeechRecognition.onend!();
        await microtasksFinished();
        await showPromise;

        // Assert.
        assertEquals(searchboxHandler.getCallCount('submitQuery'), 0);
        assertStyle(composeboxElement.$.composebox, 'display', 'flex');
        assertStyle(composeboxElement.$.voiceSearch, 'display', 'none');
        assertEquals(composeboxElement.animationState, GlowAnimationState.NONE);
      });

  test('on error shows error container for NOT_ALLOWED', async () => {
    const voiceSearchButton = getVoiceSearchButton(composeboxElement);
    voiceSearchButton!.click();
    await microtasksFinished();

    const hidePromise =
        getTransitionEndPromise(composeboxElement.$.composebox, 'opacity');

    // Simulate a 'not-allowed' error.
    mockSpeechRecognition.onerror!
        ({error: 'not-allowed'} as SpeechRecognitionErrorEvent);
    await microtasksFinished();
    await hidePromise;

    const voiceSearchElement = composeboxElement.$.voiceSearch;
    const errorContainer =
        voiceSearchElement.shadowRoot.querySelector<HTMLElement>(
            '#error-container');
    const inputElement =
        voiceSearchElement.shadowRoot.querySelector<HTMLTextAreaElement>(
            '#input');

    assertTrue(!!errorContainer);
    assertFalse(errorContainer.hidden);
    assertTrue(inputElement!.hidden);
    assertStyle(composeboxElement.$.composebox, 'opacity', '0');
    assertStyle(composeboxElement.$.voiceSearch, 'display', 'inline');
    assertEquals(
        composeboxElement.animationState, GlowAnimationState.LISTENING);
  });

  test('on error closes voice search for other errors', async () => {
    const voiceSearchButton = getVoiceSearchButton(composeboxElement);
    voiceSearchButton!.click();
    await microtasksFinished();

    const showPromise =
        getTransitionEndPromise(composeboxElement.$.composebox, 'opacity');

    // Simulate a 'network' error.
    mockSpeechRecognition.onerror!
        ({error: 'network'} as SpeechRecognitionErrorEvent);
    mockSpeechRecognition.onend!();
    await microtasksFinished();
    await showPromise;

    const voiceSearchElement = composeboxElement.$.voiceSearch;
    const errorContainer =
        voiceSearchElement.shadowRoot.querySelector<HTMLElement>(
            '#error-container');
    const inputElement =
        voiceSearchElement.shadowRoot.querySelector<HTMLTextAreaElement>(
            '#input');

    // The error container should be hidden because it's not a NOT_ALLOWED
    // error, and the voice search should close.
    assertTrue(errorContainer!.hidden);
    assertFalse(inputElement!.hidden);

    assertStyle(composeboxElement.$.composebox, 'display', 'flex');
    assertStyle(composeboxElement.$.voiceSearch, 'display', 'none');
    assertEquals(composeboxElement.animationState, GlowAnimationState.NONE);
  });

});
