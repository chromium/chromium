// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/strings.m.js';
import 'chrome://resources/cr_components/composebox/composebox.js';
import 'chrome://resources/cr_components/composebox/composebox_voice_search.js';

import type {ComposeboxElement} from 'chrome://resources/cr_components/composebox/composebox.js';
import {PageCallbackRouter, PageHandlerRemote} from 'chrome://resources/cr_components/composebox/composebox.mojom-webui.js';
import {ComposeboxProxyImpl} from 'chrome://resources/cr_components/composebox/composebox_proxy.js';
import type {ComposeboxVoiceSearchElement} from 'chrome://resources/cr_components/composebox/composebox_voice_search.js';
import {VoiceSearchAction, VoiceSearchError} from 'chrome://resources/cr_components/composebox/composebox_voice_search.js';
import {WindowProxy} from 'chrome://resources/cr_components/composebox/window_proxy.js';
import type {AudioWaveElement} from 'chrome://resources/cr_components/search/audio_wave.js';
import {GlowAnimationState, VoiceSearchState} from 'chrome://resources/cr_components/search/constants.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {$$, microtasksFinished} from 'chrome://webui-test/test_util.js';

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
    Omit<ComposeboxElement, 'transcript'|'inVoiceSearchMode'>&{
      inVoiceSearchMode: boolean,
      transcript: string,
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

suite('ComposeboxVoiceSearch', () => {
  let composeboxElement: ComposeboxElement;
  let handler: TestMock<PageHandlerRemote>;
  let searchboxHandler: TestMock<SearchboxPageHandlerRemote>;
  let windowProxy: TestMock<WindowProxy>;
  let metrics: MetricsTracker;

  suiteSetup(() => {
    loadTimeData.overrideValues({
      composeboxShowVoiceSearch: true,
      composeboxShowZps: true,
      composeboxShowTypedSuggest: true,
    });
  });

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    metrics = fakeMetricsPrivate();
    handler = installMock(
        PageHandlerRemote,
        mock => ComposeboxProxyImpl.setInstance(new ComposeboxProxyImpl(
            mock, new PageCallbackRouter(), new SearchboxPageHandlerRemote(),
            new SearchboxPageCallbackRouter())));
    assertTrue(!!handler);
    searchboxHandler = installMock(
        SearchboxPageHandlerRemote,
        mock => ComposeboxProxyImpl.getInstance().searchboxHandler = mock);
    searchboxHandler.setResultFor(
        'getPageClassification',
        Promise.resolve({metricSource: 'NTP_REALBOX'}));
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
    windowProxy.setResultMapperFor('matchMedia', () => ({
                                                   addListener() {},
                                                   addEventListener() {},
                                                   removeListener() {},
                                                   removeEventListener() {},
                                                 }));
    windowProxy.setResultFor('hasWebkitSpeechRecognition', true);

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

  function getVoiceSearchElement(composeboxElement: ComposeboxElement):
      ComposeboxVoiceSearchElement {
    const voiceSearchElement = $$<ComposeboxVoiceSearchElement>(
        composeboxElement, 'cr-composebox-voice-search');
    assertTrue(!!voiceSearchElement);
    return voiceSearchElement;
  }

  test('voice search button does not show when disabled', async () => {
    loadTimeData.overrideValues({
      composeboxShowVoiceSearch: false,
    });
    // Create element again with new loadTimeData values.
    composeboxElement = document.createElement('cr-composebox');
    document.body.appendChild(composeboxElement);
    await microtasksFinished();

    const voiceSearchButton = getVoiceSearchButton(composeboxElement);
    assertFalse(!!voiceSearchButton);

    // Restore.
    loadTimeData.overrideValues({
      composeboxShowVoiceSearch: true,
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
        assertStyle(
            getVoiceSearchElement(composeboxElement), 'display', 'inline');
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

    const voiceSearchInput = getVoiceSearchElement(composeboxElement).$.input;

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
    const hidePromise =
        getTransitionEndPromise(composeboxElement.$.composebox, 'opacity');
    const voiceSearchButton = getVoiceSearchButton(composeboxElement);
    voiceSearchButton!.click();
    await microtasksFinished();
    await hidePromise;

    assertTrue(mockSpeechRecognition.voiceSearchInProgress);
    const showPromise =
        getTransitionEndPromise(composeboxElement.$.composebox, 'opacity');

    const [callback] = await windowProxy.whenCalled('setTimeout');
    callback();
    await microtasksFinished();
    await showPromise;
    await composeboxElement.updateComplete;
    const voiceSearchElement = getVoiceSearchElement(composeboxElement);
    await voiceSearchElement.updateComplete;

    // Assert.
    assertFalse(mockSpeechRecognition.voiceSearchInProgress);
    assertEquals(searchboxHandler.getCallCount('submitQuery'), 0);
    assertStyle(composeboxElement.$.composebox, 'display', 'flex');
    assertStyle(voiceSearchElement, 'display', 'none');
    assertEquals(composeboxElement.animationState, GlowAnimationState.NONE);
  });

  test('idle timer submits voice search if final result exists', async () => {
    const hidePromise =
        getTransitionEndPromise(composeboxElement.$.composebox, 'opacity');
    const voiceSearchButton = getVoiceSearchButton(composeboxElement);
    voiceSearchButton!.click();
    await microtasksFinished();
    await hidePromise;

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
    await getVoiceSearchElement(composeboxElement).updateComplete;

    // Assert.
    assertEquals(searchboxHandler.getCallCount('submitQuery'), 1);

    assertStyle(composeboxElement.$.composebox, 'display', 'flex');
    assertStyle(getVoiceSearchElement(composeboxElement), 'display', 'none');
  });

  test(
      'idle timeout with interim result and some final result submits query',
      async () => {
        const hidePromise =
            getTransitionEndPromise(composeboxElement.$.composebox, 'opacity');
        const voiceSearchButton = getVoiceSearchButton(composeboxElement);
        voiceSearchButton!.click();
        await microtasksFinished();
        await hidePromise;

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
        const voiceSearchElement =
            getVoiceSearchElement(composeboxElement) as any;
        assertEquals(voiceSearchElement.interimResult_, 'hello');
        assertEquals(voiceSearchElement.finalResult_, 'world');
        assertEquals(voiceSearchElement.transcript_, 'helloworld');

        const [callback] = await windowProxy.whenCalled('setTimeout');
        callback();
        await microtasksFinished();
        await showPromise;
        await composeboxElement.updateComplete;
        await voiceSearchElement.updateComplete;

        // Assert.
        assertEquals(searchboxHandler.getCallCount('submitQuery'), 1);

        assertStyle(composeboxElement.$.composebox, 'display', 'flex');
        assertStyle(voiceSearchElement, 'display', 'none');
      });

  test('idle timeout with final result submits query', async () => {
    loadTimeData.overrideValues({composeboxSource: 'NewTabPage'});
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    composeboxElement = document.createElement('cr-composebox');
    document.body.appendChild(composeboxElement);

    const hidePromise =
        getTransitionEndPromise(composeboxElement.$.composebox, 'opacity');
    const voiceSearchButton = getVoiceSearchButton(composeboxElement);
    voiceSearchButton!.click();
    await microtasksFinished();
    await hidePromise;

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
    const voiceSearchElement = getVoiceSearchElement(composeboxElement) as any;
    assertEquals(voiceSearchElement.finalResult_, 'helloworld');
    assertEquals(voiceSearchElement.transcript_, 'helloworld');

    const [callback] = await windowProxy.whenCalled('setTimeout');
    callback();
    await microtasksFinished();
    await showPromise;
    await composeboxElement.updateComplete;
    await voiceSearchElement.updateComplete;
    // Assert.
    assertEquals(searchboxHandler.getCallCount('submitQuery'), 1);

    const metricName =
        'ContextualSearch.UserAction.SubmitVoiceQuery.NewTabPage';
    assertEquals(1, metrics.count(metricName, 0));
    assertEquals(1, metrics.count(metricName, true));

    assertStyle(composeboxElement.$.composebox, 'display', 'flex');
    assertStyle(voiceSearchElement, 'display', 'none');
  });

  test(
      'on end submits and exits voice search if no final result is available',
      async () => {
        (composeboxElement as any).autoSubmitVoiceSearch = true;

        const hidePromise =
            getTransitionEndPromise(composeboxElement.$.composebox, 'opacity');
        const voiceSearchButton = getVoiceSearchButton(composeboxElement);
        voiceSearchButton!.click();
        await microtasksFinished();
        await hidePromise;

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
        const voiceSearchElement =
            getVoiceSearchElement(composeboxElement) as any;
        await voiceSearchElement.updateComplete;
        assertEquals(voiceSearchElement.finalResult_, '');
        assertEquals(
            voiceSearchElement.transcript_, 'helloworld',
            'transcript should be set after result is' +
                'processed but not finalized');

        const [callback] = await windowProxy.whenCalled('setTimeout');
        callback();
        await microtasksFinished();

        assertEquals(voiceSearchElement.finalResult_, '');
        assertEquals(
            voiceSearchElement.transcript_, '',
            'transcript should be cleared after onend with no final result');

        // Assert.
        assertEquals(searchboxHandler.getCallCount('submitQuery'), 1);
        assertStyle(composeboxElement.$.composebox, 'display', 'flex');
        assertStyle(
            getVoiceSearchElement(composeboxElement), 'display', 'none');
        assertEquals(
            composeboxElement.animationState, GlowAnimationState.SUBMITTING);
      });

  test('transcript is cleared to avoid leftover past queries', async () => {
    (composeboxElement as any).autoSubmitVoiceSearch = true;

    const hidePromise =
        getTransitionEndPromise(composeboxElement.$.composebox, 'opacity');
    const voiceSearchButton = getVoiceSearchButton(composeboxElement);
    voiceSearchButton!.click();
    await microtasksFinished();
    await hidePromise;

    assertTrue(mockSpeechRecognition.voiceSearchInProgress);

    const result = createResults(2);
    Object.assign(result.results[0]![0]!, {confidence: 0, transcript: 'hello'});
    Object.assign(result.results[1]![0]!, {confidence: 0, transcript: 'world'});
    mockSpeechRecognition.onresult!(result);
    mockSpeechRecognition.onend!();

    const showPromise =
        getTransitionEndPromise(composeboxElement.$.composebox, 'opacity');

    const voiceSearchElement = getVoiceSearchElement(composeboxElement) as any;
    assertEquals(voiceSearchElement.finalResult_, '');
    assertEquals(
        voiceSearchElement.transcript_, 'helloworld',
        'transcript should be set after result is processed');
    await showPromise;
    await composeboxElement.updateComplete;
    await getVoiceSearchElement(composeboxElement).updateComplete;
    const [callback] = await windowProxy.whenCalled('setTimeout');
    callback();
    await microtasksFinished();

    assertEquals(
        voiceSearchElement.finalResult_, '',
        'finalResult should be empty as no final result was received');

    assertEquals(
        voiceSearchElement.interimResult_, '',
        'interimResult should be cleared after idle timeout');

    assertEquals(
        voiceSearchElement.transcript_, '',
        'transcript should be cleared after idle timeout');

    assertEquals(searchboxHandler.getCallCount('submitQuery'), 1);
    assertStyle(composeboxElement.$.composebox, 'display', 'flex');
    assertStyle(getVoiceSearchElement(composeboxElement), 'display', 'none');
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
    await getVoiceSearchElement(composeboxElement).updateComplete;

    const voiceSearchElement = getVoiceSearchElement(composeboxElement);
    const errorContainer = $$(voiceSearchElement, '#error-container');
    const inputElement = $$(voiceSearchElement, '#input');

    assertTrue(!!errorContainer);
    assertFalse(errorContainer.hidden);
    assertTrue(inputElement!.hidden);
    assertStyle(composeboxElement.$.composebox, 'opacity', '0');
    assertStyle(getVoiceSearchElement(composeboxElement), 'display', 'inline');
    assertEquals(
        composeboxElement.animationState, GlowAnimationState.LISTENING);
  });

  test('on error closes voice search for other errors', async () => {
    const hidePromise =
        getTransitionEndPromise(composeboxElement.$.composebox, 'opacity');
    const voiceSearchButton = getVoiceSearchButton(composeboxElement);
    voiceSearchButton!.click();
    await microtasksFinished();
    await hidePromise;

    const showPromise =
        getTransitionEndPromise(composeboxElement.$.composebox, 'opacity');

    // Simulate a 'network' error.
    mockSpeechRecognition.onerror!
        ({error: 'network'} as SpeechRecognitionErrorEvent);
    mockSpeechRecognition.onend!();
    await microtasksFinished();
    await showPromise;
    await composeboxElement.updateComplete;
    await getVoiceSearchElement(composeboxElement).updateComplete;

    const [callback] = await windowProxy.whenCalled('setTimeout');
    callback();

    const voiceSearchElement = getVoiceSearchElement(composeboxElement);
    const errorContainer = $$(voiceSearchElement, '#error-container');
    const inputElement = $$(voiceSearchElement, '#input');

    // The error container should be hidden because it's not a NOT_ALLOWED
    // error, and the voice search should close.
    assertTrue(errorContainer!.hidden);
    assertFalse(inputElement!.hidden);

    assertStyle(composeboxElement.$.composebox, 'display', 'flex');
    assertStyle(getVoiceSearchElement(composeboxElement), 'display', 'none');
    assertEquals(composeboxElement.animationState, GlowAnimationState.NONE);
  });

  test('audio wave is rendered when listening', async () => {
    const mockComposeboxElement =
        composeboxElement as unknown as MockComposebox;
    mockComposeboxElement.inVoiceSearchMode = true;
    await microtasksFinished();

    // SearchAnimatedGlow unconditionally exists
    const searchAnimatedGlow =
        composeboxElement.shadowRoot.querySelector('search-animated-glow');
    await searchAnimatedGlow!.updateComplete;
    const audioWave: AudioWaveElement|null =
        searchAnimatedGlow!.shadowRoot.querySelector('audio-wave');
    assertTrue(!!audioWave);
    mockComposeboxElement.transcript = 'foo';
    await composeboxElement.updateComplete;
    await searchAnimatedGlow!.updateComplete;
    await microtasksFinished();

    assertEquals('foo', audioWave.transcript);
  });

  test('audio wave is hidden when not listening', async () => {
    const mockComposeboxElement =
        composeboxElement as unknown as MockComposebox;
    mockComposeboxElement.inVoiceSearchMode = false;
    await microtasksFinished();

    // SearchAnimatedGlow unconditionally exists
    const searchAnimatedGlow =
        composeboxElement.shadowRoot.querySelector('search-animated-glow');
    await searchAnimatedGlow!.updateComplete;
    const audioWave: AudioWaveElement|null =
        searchAnimatedGlow!.shadowRoot.querySelector('audio-wave');
    assertTrue(!!audioWave);
  });

  test(
      'voice search container is empty without webkitSpeechRecognition API',
      async () => {
        // Temporarily remove API
        windowProxy.setResultFor('hasWebkitSpeechRecognition', false);
        await microtasksFinished();

        composeboxElement = document.createElement('cr-composebox');
        document.body.appendChild(composeboxElement);
        await microtasksFinished();

        // Query the DOM directly instead of using the `getVoiceSearchElement`
        // helper, because the helper internally asserts that the element exists
        // (assertTrue), which would cause this test to fail prematurely.
        const voiceSearchElement = $$<ComposeboxVoiceSearchElement>(
            composeboxElement, 'cr-composebox-voice-search');
        assertFalse(!!voiceSearchElement);

        // Restore API
        windowProxy.setResultFor('hasWebkitSpeechRecognition', true);
      });
});

suite('ComposeboxVoiceSearchMetrics', () => {
  let voiceSearchElement: ComposeboxVoiceSearchElement;
  let metrics: MetricsTracker;
  let handler: TestMock<PageHandlerRemote>;
  let searchboxHandler: TestMock<SearchboxPageHandlerRemote>;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    // Intercept metrics recording.
    metrics = fakeMetricsPrivate();
    handler = TestMock.fromClass(PageHandlerRemote);
    searchboxHandler = TestMock.fromClass(SearchboxPageHandlerRemote);
    searchboxHandler.setResultFor(
        'getPageClassification',
        Promise.resolve({metricSource: 'NTP_REALBOX'}));

    ComposeboxProxyImpl.setInstance(new ComposeboxProxyImpl(
        handler as unknown as PageHandlerRemote, new PageCallbackRouter(),
        searchboxHandler as unknown as SearchboxPageHandlerRemote,
        new SearchboxPageCallbackRouter()));

    // Cast to `typeof SpeechRecognition` is necessary because
    // MockSpeechRecognition only implements a subset of the actual
    // SpeechRecognition API required for testing.
    window.webkitSpeechRecognition =
        MockSpeechRecognition as unknown as typeof SpeechRecognition;

    voiceSearchElement = document.createElement('cr-composebox-voice-search');

    document.body.appendChild(voiceSearchElement);
    await microtasksFinished();
  });

  test('Records SUCCESS and SUBMITTED metrics on final result', async () => {
    // Trigger: Simulate receiving the final voice result.
    (voiceSearchElement as any).onFinalResult_('hello world');
    await microtasksFinished();
    // Verify: Action logged QUERY_SUBMITTED.
    assertEquals(
        1,
        metrics.count(
            'VoiceSearch.Action.NTP_REALBOX',
            VoiceSearchAction.QUERY_SUBMITTED));

    // Verify: State logged SUCCESSFUL_TRANSCRIPT.
    assertEquals(
        1,
        metrics.count(
            'VoiceSearch.State.NTP_REALBOX',
            VoiceSearchState.SUCCESSFUL_TRANSCRIPT));
  });

  test('Records CANCELED metrics on close button click', async () => {
    // Trigger: Simulate user clicking close.
    (voiceSearchElement as any).onCloseClick_();
    await microtasksFinished();

    // Verify: Action logged CLOSED_BY_USER.
    assertEquals(
        1,
        metrics.count(
            'VoiceSearch.Action.NTP_REALBOX',
            VoiceSearchAction.CLOSED_BY_USER));

    // Verify: State logged VOICE_SEARCH_CANCELED.
    assertEquals(
        1,
        metrics.count(
            'VoiceSearch.State.NTP_REALBOX',
            VoiceSearchState.VOICE_SEARCH_CANCELED));
  });

  test('Records ERROR metrics on API error event', async () => {
    // Change parameters to test if dynamic concatenation works.
    searchboxHandler.setResultFor(
        'getPageClassification',
        Promise.resolve({metricSource: 'CO_BROWSING_COMPOSEBOX'}));
    await microtasksFinished();

    // Trigger: Simulate underlying API throwing an error (network).
    const errorEvent = new Event('error') as any;
    errorEvent.error = 'network';
    (voiceSearchElement as any).voiceRecognition_.onerror(errorEvent);

    await microtasksFinished();
    // Verify: Errors logged NETWORK.
    assertEquals(
        1,
        metrics.count(
            'VoiceSearch.Errors.CO_BROWSING_COMPOSEBOX',
            VoiceSearchError.NETWORK));
  });

  test('Records ERROR_NON_CANCELING state for NOT_ALLOWED error', async () => {
    // Trigger: Simulate permission denied (not-allowed).
    const errorEvent = new Event('error') as any;
    errorEvent.error = 'not-allowed';
    (voiceSearchElement as any).voiceRecognition_.onerror(errorEvent);

    // Call onEnd_ to simulate recognition ending, which is when the State is
    // recorded.
    (voiceSearchElement as any).onEnd_();
    await microtasksFinished();

    // Verify: State logged a non-canceling error (ERROR_NON_CANCELING).
    assertEquals(
        1,
        metrics.count(
            'VoiceSearch.State.NTP_REALBOX',
            VoiceSearchState.VOICE_SEARCH_ERROR));
  });

  test('Records ERROR_CANCELING state for other errors in onEnd_', async () => {
    // Trigger: Simulate network error.
    const errorEvent = new Event('error') as any;
    errorEvent.error = 'network';
    (voiceSearchElement as any).voiceRecognition_.onerror(errorEvent);

    // Call onEnd_ to simulate recognition ending.
    (voiceSearchElement as any).onEnd_();
    await microtasksFinished();

    // Verify: State logged a canceling error (ERROR_CANCELING).
    assertEquals(
        1,
        metrics.count(
            'VoiceSearch.State.NTP_REALBOX',
            VoiceSearchState.VOICE_SEARCH_ERROR_AND_CANCELED));
  });

  test('Records NO_MATCH error on nomatch event', async () => {
    // Trigger: Simulate no match (onnomatch).
    (voiceSearchElement as any)
        .voiceRecognition_.onnomatch(new Event('nomatch'));

    await microtasksFinished();
    // Verify: Errors logged NO_MATCH.
    assertEquals(
        1,
        metrics.count(
            'VoiceSearch.Errors.NTP_REALBOX', VoiceSearchError.NO_MATCH));
  });

  test('Records Action metrics on link interactions', async () => {
    const mockRetryEvent = new MouseEvent('click');
    mockRetryEvent.stopPropagation = () => {};
    (voiceSearchElement as any).onTryAgainClick_(mockRetryEvent);
    await microtasksFinished();

    assertEquals(
        1,
        metrics.count(
            'VoiceSearch.Action.NTP_REALBOX',
            VoiceSearchAction.RETRY_BY_TRY_AGAIN_CLICKED));

    const mockLinkEvent = new MouseEvent('click');
    mockLinkEvent.preventDefault = () => {};

    Object.defineProperty(
        mockLinkEvent, 'currentTarget',
        {value: {href: 'https://support.google.com/'}});

    (voiceSearchElement as any).onLinkClick_(mockLinkEvent);
    await microtasksFinished();

    assertEquals(
        1,
        metrics.count(
            'VoiceSearch.Action.NTP_REALBOX',
            VoiceSearchAction.SUPPORT_LINK_CLICKED));

    (voiceSearchElement as any).state_ = -1;
    (voiceSearchElement as any).voiceRecognition_.abort();
    await microtasksFinished();
  });

  test(
      'Records specific errors in onEnd_ based on fallback state', async () => {
        // Trigger: Force set internal state to STARTED and call onEnd_.
        (voiceSearchElement as any).state_ = 0;  // State.STARTED
        (voiceSearchElement as any).onEnd_();
        await microtasksFinished();
        // Verify: Because it ended unexpectedly during STARTED, it should log
        // an AUDIO_CAPTURE error.
        assertEquals(
            1,
            metrics.count(
                'VoiceSearch.Errors.NTP_REALBOX',
                VoiceSearchError.AUDIO_CAPTURE));
      });
});
