// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/strings.m.js';
import 'chrome://resources/cr_components/composebox/composebox.js';

import type {ComposeboxElement} from 'chrome://resources/cr_components/composebox/composebox.js';
// import {VoiceSearchAction} from
// 'chrome://resources/cr_components/composebox/composebox.js';
import {PageCallbackRouter, PageHandlerRemote} from 'chrome://resources/cr_components/composebox/composebox.mojom-webui.js';
import {ComposeboxProxyImpl} from 'chrome://resources/cr_components/composebox/composebox_proxy.js';
import {WindowProxy} from 'chrome://resources/cr_components/composebox/window_proxy.js';
import type {AudioWaveElement} from 'chrome://resources/cr_components/search/audio_wave.js';
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

// Exposing private/protected vars as public in these components:
type MockComposebox =
    Omit<ComposeboxElement, 'transcript_'|'inVoiceSearchMode_'>&{
      inVoiceSearchMode_: boolean,
      transcript_: string,
    };

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
    searchboxHandler.setResultFor('getInputState', Promise.resolve({
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

    windowProxy = installMock(WindowProxy);
    windowProxy.setResultFor('setTimeout', 0);

    composeboxElement = document.createElement('cr-composebox');
    document.body.appendChild(composeboxElement);
    window.webkitSpeechRecognition =
        MockSpeechRecognition as unknown as typeof SpeechRecognition;
  });

  function getVoiceSearchButton(composeboxElement: ComposeboxElement):
      HTMLElement|null {
    return composeboxElement.shadowRoot.querySelector<HTMLElement>(
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
    const [callback] = await windowProxy.whenCalled('setTimeout');
    callback();

    // Act.
    mockSpeechRecognition.onresult!(result2);
    await microtasksFinished();

    // Speech recognition overrides existing composebox input.
    assertEquals('hellogoodbye', voiceSearchInput.value);
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
    await composeboxElement.updateComplete;
    await composeboxElement.$.voiceSearch.updateComplete;

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
    Object.assign(result.results[0]![0]!, {confidence: 1, transcript: 'hello'});
    Object.assign(result.results[1]![0]!, {confidence: 1, transcript: 'world'});
    Object.assign(result.results[0]!, {isFinal: false});
    Object.assign(result.results[1]!, {isFinal: true});
    (result as any).resultIndex = 1;
    const showPromise =
        getTransitionEndPromise(composeboxElement.$.composebox, 'opacity');
    // Act.
    mockSpeechRecognition.onresult!(result);

    await microtasksFinished();
    await showPromise;
    await composeboxElement.updateComplete;
    await composeboxElement.$.voiceSearch.updateComplete;

    // Assert.
    assertEquals(searchboxHandler.getCallCount('submitQuery'), 1);

    assertStyle(composeboxElement.$.composebox, 'display', 'flex');
    assertStyle(composeboxElement.$.voiceSearch, 'display', 'none');
  });

  test(
      'idle timeout with interim result and some final result submits query',
      async () => {
        const voiceSearchButton = getVoiceSearchButton(composeboxElement);
        voiceSearchButton!.click();
        await microtasksFinished();

        assertTrue(mockSpeechRecognition.voiceSearchInProgress);

        const result = createResults(2);
        // Confidence 0 produces interim result.
        Object.assign(
            result.results[0]![0]!, {confidence: 0, transcript: 'hello'});
        Object.assign(
            result.results[1]![0]!, {confidence: 1, transcript: 'world'});

        const showPromise =
            getTransitionEndPromise(composeboxElement.$.composebox, 'opacity');

        // Act.
        mockSpeechRecognition.onresult!(result);
        assertEquals(
            (composeboxElement.$.voiceSearch as any).interimResult_, 'hello');
        assertEquals(
            (composeboxElement.$.voiceSearch as any).finalResult_, 'world');
        assertEquals(
            (composeboxElement.$.voiceSearch as any).transcript_, 'helloworld');

        const [callback] = await windowProxy.whenCalled('setTimeout');
        callback();
        await microtasksFinished();
        await showPromise;
        await composeboxElement.updateComplete;
        await composeboxElement.$.voiceSearch.updateComplete;

        // Assert.
        assertEquals(searchboxHandler.getCallCount('submitQuery'), 1);

        assertStyle(composeboxElement.$.composebox, 'display', 'flex');
        assertStyle(composeboxElement.$.voiceSearch, 'display', 'none');
      });

  test('idle timeout with final result submits query', async () => {
    const voiceSearchButton = getVoiceSearchButton(composeboxElement);
    voiceSearchButton!.click();
    await microtasksFinished();

    assertTrue(mockSpeechRecognition.voiceSearchInProgress);

    const result = createResults(2);
    Object.assign(result.results[0]![0]!, {confidence: 1, transcript: 'hello'});
    Object.assign(result.results[1]![0]!, {confidence: 1, transcript: 'world'});
    Object.assign(result.results[0]!, {isFinal: false});
    Object.assign(result.results[1]!, {isFinal: true});
    const showPromise =
        getTransitionEndPromise(composeboxElement.$.composebox, 'opacity');

    // Act.
    mockSpeechRecognition.onresult!(result);
    assertEquals(
        (composeboxElement.$.voiceSearch as any).finalResult_, 'helloworld');
    assertEquals(
        (composeboxElement.$.voiceSearch as any).transcript_, 'helloworld');

    const [callback] = await windowProxy.whenCalled('setTimeout');
    callback();
    await microtasksFinished();
    await showPromise;
    await composeboxElement.updateComplete;
    await composeboxElement.$.voiceSearch.updateComplete;
    // Assert.
    assertEquals(searchboxHandler.getCallCount('submitQuery'), 1);

    assertStyle(composeboxElement.$.composebox, 'display', 'flex');
    assertStyle(composeboxElement.$.voiceSearch, 'display', 'none');
  });

  test(
      'on end submits and exits voice search if no final result is available',
      async () => {
        (composeboxElement as any).autoSubmitVoiceSearch = true;

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
        await composeboxElement.updateComplete;
        await composeboxElement.$.voiceSearch.updateComplete;
        assertEquals((composeboxElement.$.voiceSearch as any).finalResult_, '');
        assertEquals(
            (composeboxElement.$.voiceSearch as any).transcript_, 'helloworld',
            'transcript should be set after result is' +
                'processed but not finalized');

        const [callback] = await windowProxy.whenCalled('setTimeout');
        callback();
        await microtasksFinished();

        assertEquals((composeboxElement.$.voiceSearch as any).finalResult_, '');
        assertEquals(
            (composeboxElement.$.voiceSearch as any).transcript_, '',
            'transcript should be cleared after onend with no final result');

        // Assert.
        assertEquals(searchboxHandler.getCallCount('submitQuery'), 1);
        assertStyle(composeboxElement.$.composebox, 'display', 'flex');
        assertStyle(composeboxElement.$.voiceSearch, 'display', 'none');
        assertEquals(
            composeboxElement.animationState, GlowAnimationState.SUBMITTING);
      });

  test('transcript is cleared to avoid leftover past queries', async () => {
    (composeboxElement as any).autoSubmitVoiceSearch = true;

    const voiceSearchButton = getVoiceSearchButton(composeboxElement);
    voiceSearchButton!.click();
    await microtasksFinished();

    assertTrue(mockSpeechRecognition.voiceSearchInProgress);

    const result = createResults(2);
    Object.assign(result.results[0]![0]!, {confidence: 0, transcript: 'hello'});
    Object.assign(result.results[1]![0]!, {confidence: 0, transcript: 'world'});
    mockSpeechRecognition.onresult!(result);
    mockSpeechRecognition.onend!();

    const showPromise =
        getTransitionEndPromise(composeboxElement.$.composebox, 'opacity');

    assertEquals((composeboxElement.$.voiceSearch as any).finalResult_, '');
    assertEquals(
        (composeboxElement.$.voiceSearch as any).transcript_, 'helloworld',
        'transcript should be set after result is processed');
    await showPromise;
    await composeboxElement.updateComplete;
    await composeboxElement.$.voiceSearch.updateComplete;
    const [callback] = await windowProxy.whenCalled('setTimeout');
    callback();
    await microtasksFinished();

    assertEquals(
        (composeboxElement.$.voiceSearch as any).finalResult_, '',
        'finalResult should be empty as no final result was received');

    assertEquals(
        (composeboxElement.$.voiceSearch as any).interimResult_, '',
        'interimResult should be cleared after idle timeout');

    assertEquals(
        (composeboxElement.$.voiceSearch as any).transcript_, '',
        'transcript should be cleared after idle timeout');

    assertEquals(searchboxHandler.getCallCount('submitQuery'), 1);
    assertStyle(composeboxElement.$.composebox, 'display', 'flex');
    assertStyle(composeboxElement.$.voiceSearch, 'display', 'none');
    assertEquals(
        composeboxElement.animationState, GlowAnimationState.SUBMITTING);
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
    await composeboxElement.updateComplete;
    await composeboxElement.$.voiceSearch.updateComplete;

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
    await composeboxElement.updateComplete;
    await composeboxElement.$.voiceSearch.updateComplete;

    const [callback] = await windowProxy.whenCalled('setTimeout');
    callback();

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

  test('audio wave is rendered when listening', async () => {
    const mockComposeboxElement =
        composeboxElement as unknown as MockComposebox;
    mockComposeboxElement.inVoiceSearchMode_ = true;
    await microtasksFinished();

    // SearchAnimatedGlow unconditionally exists
    const searchAnimatedGlow =
        composeboxElement.shadowRoot.querySelector('search-animated-glow');
    await searchAnimatedGlow!.updateComplete;
    const audioWave: AudioWaveElement|null =
        searchAnimatedGlow!.shadowRoot.querySelector('audio-wave');
    assertTrue(!!audioWave);
    mockComposeboxElement.transcript_ = 'foo';
    await composeboxElement.updateComplete;
    await searchAnimatedGlow!.updateComplete;
    await microtasksFinished();

    assertEquals('foo', audioWave.transcript);
  });

  test('audio wave is hidden when not listening', async () => {
    const mockComposeboxElement =
        composeboxElement as unknown as MockComposebox;
    mockComposeboxElement.inVoiceSearchMode_ = false;
    await microtasksFinished();

    // SearchAnimatedGlow unconditionally exists
    const searchAnimatedGlow =
        composeboxElement.shadowRoot.querySelector('search-animated-glow');
    await searchAnimatedGlow!.updateComplete;
    const audioWave: AudioWaveElement|null =
        searchAnimatedGlow!.shadowRoot.querySelector('audio-wave');
    assertTrue(!!audioWave);
  });
});
