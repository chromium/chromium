// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/strings.m.js';
import 'chrome://resources/cr_components/composebox/composebox.js';
import 'chrome://resources/cr_components/composebox/composebox_voice_search.js';

import type {ComposeboxElement} from 'chrome://resources/cr_components/composebox/composebox.js';
import {PageCallbackRouter, PageHandlerRemote} from 'chrome://resources/cr_components/composebox/composebox.mojom-webui.js';
import {ComposeboxProxyImpl} from 'chrome://resources/cr_components/composebox/composebox_proxy.js';
import type {ComposeboxVoiceSearchElement} from 'chrome://resources/cr_components/composebox/composebox_voice_search.js';
import {VoiceSearchAction, VoiceSearchError} from 'chrome://resources/cr_components/composebox/composebox_voice_search.js';
import {WindowProxy} from 'chrome://resources/cr_components/composebox/window_proxy.js';
import type {AudioWaveElement} from 'chrome://resources/cr_components/search/audio_wave.js';
import type {RecordingWaveElement} from 'chrome://resources/cr_components/search/recording_wave.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeMediaQueryList} from 'chrome://webui-test/fake_media_query_list.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {$$, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {assertStyle, disableTransitionsRecursively, installMock, MockSpeechRecognition, mockSpeechRecognition} from './composebox_test_utils.js';
import type {MockComposebox, MockComposeboxVoiceSearch} from './composebox_test_utils.js';

// Returns a promise that resolves when CSS style has transitioned.
function getTransitionEndPromise(
    element: HTMLElement, _property?: string): Promise<void> {
  element.style.transition = 'none';
  return Promise.resolve();
}

function createResults(n: number): SpeechRecognitionEvent {
  const results = Array.from({length: n}, () => {
    return {
      0: {transcript: ''},
      length: 1,
      isFinal: true,
    } as unknown as SpeechRecognitionResult;
  }) as unknown as SpeechRecognitionResultList;

  return {
    type: 'result',
    resultIndex: 0,
    results: results,
  } as unknown as SpeechRecognitionEvent;
}

suite('ComposeboxVoiceSearchRecognition', () => {
  let composeboxElement: ComposeboxElement;
  let handler: TestMock<PageHandlerRemote>;
  let searchboxHandler: TestMock<SearchboxPageHandlerRemote>;
  let windowProxy: TestMock<WindowProxy>;
  let metrics: MetricsTracker;

  suiteSetup(() => {
    loadTimeData.overrideValues({
      composeboxShowZps: true,
      composeboxShowTypedSuggest: true,
    });
  });

  setup(async () => {
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
    // <if expr="not is_android">
    searchboxHandler.setResultMapperFor(
        'getSmartTabSharingActive', () => Promise.resolve({active: false}));
    // </if>
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
    windowProxy.setResultMapperFor(
        'matchMedia', (query: string) => new FakeMediaQueryList(query));
    windowProxy.setResultFor('hasWebkitSpeechRecognition', true);

    loadTimeData.overrideValues({
      voiceSearchCoherenceComposeboxesEnabled: false,
      voiceSearchCoherenceAnySearchboxExperimentEnabled: false,
      voiceSearchCoherenceCobrowsingComposeboxEnabled: false,
      isSystemVoiceSearchEnabled: false,
    });

    windowProxy.setResultMapperFor(
        'createSpeechRecognition',
        () => new MockSpeechRecognition() as unknown as SpeechRecognition);

    await createComposeboxElement();
  });

  async function createComposeboxElement(showVoiceSearch: boolean = true) {
    if (composeboxElement && composeboxElement.parentNode) {
      composeboxElement.remove();
    }
    composeboxElement = document.createElement('cr-composebox');
    composeboxElement.showVoiceSearch = showVoiceSearch;
    composeboxElement.composeboxSource =
        loadTimeData.valueExists('composeboxSource') ?
        loadTimeData.getString('composeboxSource') :
        'NTP_REALBOX';
    document.body.appendChild(composeboxElement);
    await microtasksFinished();
    disableTransitionsRecursively(composeboxElement);
  }

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

  test('updates input correctly when voice search is stopped', async () => {
    // Set initial input.
    composeboxElement.input = 'original text';

    // Open voice search.
    const voiceSearchButton =
        composeboxElement.shadowRoot.querySelector<HTMLElement>(
            '#voiceSearchButton');
    assertTrue(!!voiceSearchButton);
    voiceSearchButton.click();
    await microtasksFinished();

    const voiceSearchElement = composeboxElement.shadowRoot.querySelector(
        'cr-composebox-voice-search');
    assertTrue(!!voiceSearchElement);

    // Case 1: Empty transcript should keep existing input.
    voiceSearchElement.dispatchEvent(
        new CustomEvent('recording-stopped', {detail: ''}));
    await microtasksFinished();

    assertEquals('original text', composeboxElement.input);
    assertFalse(composeboxElement.inVoiceSearchMode);

    // Case 2: Non-empty transcript should clobber existing input.
    voiceSearchButton.click();
    await microtasksFinished();

    voiceSearchElement.dispatchEvent(new CustomEvent(
        'recording-stopped', {detail: 'new voice search query'}));
    await microtasksFinished();

    assertEquals('new voice search query', composeboxElement.input);
    assertFalse(composeboxElement.inVoiceSearchMode);
  });

  test(
      'Records QUERY_SUBMITTED action and fires event on submit click',
      async () => {
        loadTimeData.overrideValues({
          composeboxSource: 'NTP_REALBOX',
          voiceSearchCoherenceComposeboxesEnabled: true,
        });

        document.body.innerHTML = window.trustedTypes!.emptyHTML;
        await createComposeboxElement();

        const voiceSearchElement = getVoiceSearchElement(composeboxElement);

        voiceSearchElement.start();
        await microtasksFinished();

        assertTrue(mockSpeechRecognition.voiceSearchInProgress);

        // Simulate a voice recognition result containing both final and interim
        // text. This allows the component to internally update finalResult_ and
        // interimResult_.
        const result = createResults(2);
        Object.assign(
            result.results[0]![0]!, {confidence: 1, transcript: 'hello'});
        Object.assign(
            result.results[1]![0]!, {confidence: 0, transcript: ' world'});
        mockSpeechRecognition.onresult!(result);
        await microtasksFinished();

        let firedTranscript = '';
        voiceSearchElement.addEventListener(
            'voice-search-final-result', (e: Event) => {
              firedTranscript = (e as CustomEvent<string>).detail;
            });

        // Simulate a user clicking the Submit button.
        const submitButton =
            voiceSearchElement.shadowRoot.querySelector<HTMLElement>(
                '#submitButton');
        assertTrue(!!submitButton);
        submitButton.dispatchEvent(new CustomEvent('submit-click'));
        await microtasksFinished();

        // Verify the emitted transcript is cleanly concatenated and trimmed.
        assertEquals('hello world', firedTranscript);

        // Verify that the voice search engine has successfully stopped.
        assertFalse(mockSpeechRecognition.voiceSearchInProgress);

        assertEquals(
            1,
            metrics.count(
                'VoiceSearch.Action.NTP_REALBOX',
                VoiceSearchAction.QUERY_SUBMITTED));

        // Clean up internal state.
        voiceSearchElement['voiceModeEndCleanup_']();
        await microtasksFinished();
      });

  test(
      'clicking voice search twice does not start speech recognition twice',
      async () => {
        const voiceSearchButton = getVoiceSearchButton(composeboxElement);
        assertTrue(!!voiceSearchButton);
        voiceSearchButton.click();
        await microtasksFinished();

        assertEquals(mockSpeechRecognition.startCount, 1);

        // Click again.
        voiceSearchButton.click();
        await microtasksFinished();

        // Should still be 1.
        assertEquals(mockSpeechRecognition.startCount, 1);
      });

  test('calling start() when already started does nothing', async () => {
    const voiceSearchButton = getVoiceSearchButton(composeboxElement);
    assertTrue(!!voiceSearchButton);
    voiceSearchButton.click();
    await microtasksFinished();

    assertEquals(mockSpeechRecognition.startCount, 1);

    const voiceSearchElement = getVoiceSearchElement(composeboxElement);
    // Call start again directly on the element.
    voiceSearchElement.start();

    // Should still be 1.
    assertEquals(mockSpeechRecognition.startCount, 1);
  });

  test(
      'calling start() immediately after abort() safely restarts recognition',
      async () => {
        const voiceSearchElement = getVoiceSearchElement(composeboxElement);
        const mockVoiceSearch =
            voiceSearchElement as unknown as MockComposeboxVoiceSearch;

        // Start 1st time.
        voiceSearchElement.start();
        await microtasksFinished();
        assertEquals(mockSpeechRecognition.startCount, 1);
        assertTrue(mockVoiceSearch.voiceRecognition_.voiceSearchInProgress);

        const originalAbort = mockSpeechRecognition.abort;
        let onEndCalled = false;
        let triggerOnEnd: any = null;
        mockSpeechRecognition.abort = function() {
          this.voiceSearchInProgress = false;
          triggerOnEnd = () => {
            onEndCalled = true;
            this.onend!();
          };
        };

        // Call onCloseClick_ (which calls abort()).
        mockVoiceSearch.onCloseClick_();

        // At this point, the mock has aborted, but onend has not been called
        // yet. So recognitionActive_ is still true and call start() again.
        voiceSearchElement.start();

        // The startCount should still be 1, because the start has been queued.
        assertEquals(mockSpeechRecognition.startCount, 1);

        // Manually trigger the deferred onend.
        assertTrue(!!triggerOnEnd);
        triggerOnEnd!();
        await microtasksFinished();

        // After onend fires, the queued start should execute, bringing
        // startCount to 2.
        assertEquals(mockSpeechRecognition.startCount, 2);
        assertTrue(onEndCalled);

        // Restore original abort.
        mockSpeechRecognition.abort = originalAbort;

        // Cleanup.
        mockVoiceSearch.state_ = -1;
        mockVoiceSearch.voiceRecognition_.abort();
        await microtasksFinished();
      });

  test('on result updates the searchbox input', async () => {
    const voiceSearchButton = getVoiceSearchButton(composeboxElement);
    assertTrue(!!voiceSearchButton);
    voiceSearchButton.click();
    await microtasksFinished();

    const voiceSearchElement = getVoiceSearchElement(composeboxElement);

    const result = createResults(2);
    Object.assign(result.results[0]![0]!, {transcript: 'hello'});
    Object.assign(result.results[1]![0]!, {transcript: 'world'});

    // Act.
    mockSpeechRecognition.onresult!(result);
    await microtasksFinished();

    const voiceSearchInput = voiceSearchElement.$.input;

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

  test('idle timer submits voice search if final result exists', async () => {
    const hidePromise =
        getTransitionEndPromise(composeboxElement.$.composebox, 'opacity');
    const voiceSearchButton = getVoiceSearchButton(composeboxElement);
    assertTrue(!!voiceSearchButton);
    voiceSearchButton.click();
    await microtasksFinished();
    await hidePromise;

    assertTrue(mockSpeechRecognition.voiceSearchInProgress);

    const result = createResults(2);
    Object.assign(result.results[0]![0]!, {confidence: 1, transcript: 'hello'});
    Object.assign(result.results[1]![0]!, {confidence: 1, transcript: 'world'});
    Object.assign(result.results[0]!, {isFinal: false});
    Object.assign(result.results[1]!, {isFinal: true});
    Object.defineProperty(result, 'resultIndex', {value: 1});
    const showPromise =
        getTransitionEndPromise(composeboxElement.$.composebox, 'opacity');
    // Act.
    mockSpeechRecognition.onresult!(result);

    const [callback] = await windowProxy.whenCalled('setTimeout');
    callback();

    await microtasksFinished();
    await showPromise;

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
        assertTrue(!!voiceSearchButton);
        voiceSearchButton.click();
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
    searchboxHandler.setResultFor(
        'getPageClassification',
        Promise.resolve({metricSource: 'NewTabPage'}));
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    await createComposeboxElement();

    const hidePromise =
        getTransitionEndPromise(composeboxElement.$.composebox, 'opacity');
    const voiceSearchButton = getVoiceSearchButton(composeboxElement);
    assertTrue(!!voiceSearchButton);
    voiceSearchButton.click();
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
      'on error keeps voice search open and shows error' +
          'container for all errors',
      async () => {
        const hidePromise =
            getTransitionEndPromise(composeboxElement.$.composebox, 'opacity');
        const voiceSearchButton = getVoiceSearchButton(composeboxElement);
        assertTrue(!!voiceSearchButton);
        voiceSearchButton.click();
        await microtasksFinished();
        await hidePromise;

        mockSpeechRecognition.onerror!(new SpeechRecognitionErrorEvent(
            'error', {message: '', error: 'network'}));
        await microtasksFinished();

        const voiceSearchElement = getVoiceSearchElement(composeboxElement);
        const errorContainer = $$(voiceSearchElement, '#error-container');
        const inputElement = $$(voiceSearchElement, '#input');

        // Assert: The error container should be visible for ALL errors now.
        assertTrue(!!errorContainer);
        assertFalse(errorContainer.hidden);
        assertStyle(inputElement!, 'opacity', '0');

        // Assert: The UI should remain open (voice search visible,
        // composebox hidden).
        assertStyle(composeboxElement.$.composebox, 'display', 'none');
        assertStyle(voiceSearchElement, 'display', 'block');

        // Assert: The error message is populated directly from loadTimeData.
        assertEquals(
            loadTimeData.getString('networkError'),
            (voiceSearchElement as any).errorMessage_);
      });

  test('onEnd_ triggers AUDIO_CAPTURE error if state is STARTED', async () => {
    const voiceSearchButton = getVoiceSearchButton(composeboxElement);
    assertTrue(!!voiceSearchButton);
    voiceSearchButton.click();
    await microtasksFinished();

    // State is STARTED. Trigger end event directly, skipping audio/speech
    // start.
    mockSpeechRecognition.onend!();
    await microtasksFinished();

    const voiceSearchElement = getVoiceSearchElement(composeboxElement) as any;
    // Assert: The onEnd_ fallback routing works.
    assertEquals(
        VoiceSearchError.AUDIO_CAPTURE, voiceSearchElement.detailedError_);
  });

  test('ABORTED error is ignored and does not overwrite state', async () => {
    const voiceSearchButton = getVoiceSearchButton(composeboxElement);
    assertTrue(!!voiceSearchButton);
    voiceSearchButton.click();
    await microtasksFinished();

    // Simulate receiving a system ABORTED error.
    mockSpeechRecognition.onerror!(new SpeechRecognitionErrorEvent(
        'error', {message: '', error: 'network'}));
    await microtasksFinished();

    const voiceSearchElement = getVoiceSearchElement(composeboxElement) as any;
    // Assert: The component should guard against ABORTED and not record it.
    assertNotEquals(
        VoiceSearchError.ABORTED, voiceSearchElement.detailedError_);
  });

  test('audio wave is rendered when listening', async () => {
    loadTimeData.overrideValues({
      voiceSearchCoherenceComposeboxesEnabled: false,
    });
    await createComposeboxElement();

    const mockComposeboxElement =
        composeboxElement as unknown as MockComposebox;
    mockComposeboxElement.inVoiceSearchMode = true;
    await microtasksFinished();

    // SearchAnimatedGlow unconditionally exists
    const searchAnimatedGlow =
        mockComposeboxElement.shadowRoot.querySelector('search-animated-glow');
    await microtasksFinished();

    const audioWave: AudioWaveElement|null =
        searchAnimatedGlow!.shadowRoot.querySelector('audio-wave');
    assertTrue(!!audioWave, 'Audio wave should be shown');
    const recordingWave: RecordingWaveElement|null =
        searchAnimatedGlow!.shadowRoot.querySelector('recording-wave');
    assertFalse(!!recordingWave, 'Recording wave should not be shown');

    const voiceSearchElement = getVoiceSearchElement(composeboxElement);
    assertTrue(!!voiceSearchElement, 'Voice search element should exist');

    const stopButton =
        voiceSearchElement.shadowRoot.querySelector('#stopButton');
    assertFalse(!!stopButton, 'Stop button should not be shown');

    const submitButton =
        voiceSearchElement.shadowRoot.querySelector('#submitButton');
    assertFalse(!!submitButton, 'Submit button should not be shown');

    mockComposeboxElement.transcript = 'foo';
    await microtasksFinished();

    assertEquals('foo', audioWave.transcript);
  });

  test('audio wave is hidden when not listening', async () => {
    loadTimeData.overrideValues({
      voiceSearchCoherenceComposeboxesEnabled: false,
    });
    await createComposeboxElement();

    composeboxElement.inVoiceSearchMode = false;
    await microtasksFinished();

    // SearchAnimatedGlow unconditionally exists
    const searchAnimatedGlow =
        composeboxElement.shadowRoot.querySelector('search-animated-glow');
    await microtasksFinished();
    const audioWave: AudioWaveElement|null =
        searchAnimatedGlow!.shadowRoot.querySelector('audio-wave');
    assertTrue(!!audioWave);
    assertFalse(audioWave.isListening);

    const recordingWave: RecordingWaveElement|null =
        searchAnimatedGlow!.shadowRoot.querySelector('recording-wave');
    assertFalse(!!recordingWave);

    const voiceSearchElement = getVoiceSearchElement(composeboxElement);
    assertTrue(!!voiceSearchElement, 'Voice search element should exist');

    const stopButton =
        voiceSearchElement.shadowRoot.querySelector('#stopButton');
    assertFalse(!!stopButton, 'Stop button should not be shown');

    const submitButton =
        voiceSearchElement.shadowRoot.querySelector('#submitButton');
    assertFalse(!!submitButton, 'Submit button should not be shown');
  });

  test('recording wave is rendered when listening for composebox', async () => {
    loadTimeData.overrideValues({
      voiceSearchCoherenceComposeboxesEnabled: true,
    });
    await createComposeboxElement();

    composeboxElement.inVoiceSearchMode = true;
    await microtasksFinished();

    // SearchAnimatedGlow unconditionally exists
    const searchAnimatedGlow =
        composeboxElement.shadowRoot.querySelector('search-animated-glow');
    await microtasksFinished();

    const recordingWave: RecordingWaveElement|null =
        searchAnimatedGlow!.shadowRoot.querySelector('recording-wave');
    assertTrue(!!recordingWave, 'Recording wave should be shown');

    const audioWave: AudioWaveElement|null =
        searchAnimatedGlow!.shadowRoot.querySelector('audio-wave');
    assertFalse(!!audioWave, 'Audio wave should not be shown');

    const voiceSearchElement = getVoiceSearchElement(composeboxElement);
    assertTrue(!!voiceSearchElement, 'Voice search element should exist');

    const stopButton =
        voiceSearchElement.shadowRoot.querySelector('#stopButton');
    assertTrue(!!stopButton, 'Stop button should be shown');

    const submitButton =
        voiceSearchElement.shadowRoot.querySelector('#submitButton');
    assertTrue(!!submitButton, 'Submit button should be shown');
  });
});
