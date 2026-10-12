// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/strings.m.js';
import './test_composebox_mixin.js';
import 'chrome://resources/cr_components/composebox/composebox_voice_search.js';

import {PageHandlerRemote} from 'chrome://resources/cr_components/composebox/composebox.mojom-webui.js';
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
import type {TestComposeboxMixinElement} from './test_composebox_mixin.js';

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
  let composeboxElement: TestComposeboxMixinElement;
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
            mock, new SearchboxPageHandlerRemote(),
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
      androidSpeechRecognition: false,
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
    composeboxElement = document.createElement('test-composebox-mixin');
    composeboxElement.showVoiceSearch = showVoiceSearch;
    composeboxElement.composeboxSource =
        loadTimeData.valueExists('composeboxSource') ?
        loadTimeData.getString('composeboxSource') :
        'NTP_REALBOX';
    document.body.appendChild(composeboxElement);
    await microtasksFinished();
    disableTransitionsRecursively(composeboxElement);
  }

  function getVoiceSearchButton(composeboxElement: TestComposeboxMixinElement):
      HTMLElement|null {
    return composeboxElement.shadowRoot.querySelector<HTMLElement>(
        '#voiceSearchButton');
  }

  function getVoiceSearchElement(composeboxElement: TestComposeboxMixinElement):
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

    const voiceSearchInput =
        voiceSearchElement.shadowRoot.querySelector<HTMLElement>('#input')!;

    assertEquals('helloworld', voiceSearchInput.textContent?.trim());

    // Reset the composebox input.
    const resetResult = createResults(1);
    Object.assign(resetResult.results[0]![0]!, {transcript: 'test'});
    mockSpeechRecognition.onresult!(resetResult);
    await voiceSearchElement.updateComplete;
    assertEquals('test', voiceSearchInput.textContent?.trim());
    await microtasksFinished();

    const result2 = createResults(2);
    Object.assign(result2.results[0]![0]!, {transcript: 'hello'});
    Object.assign(result2.results[1]![0]!, {transcript: 'goodbye'});

    // Act.
    mockSpeechRecognition.onresult!(result2);
    await microtasksFinished();
    // Speech recognition overrides existing composebox input.
    assertEquals('hellogoodbye', voiceSearchInput.textContent?.trim());
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
        VoiceSearchError.AUDIO_CAPTURE, voiceSearchElement.detailedError);
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
        VoiceSearchError.ABORTED, voiceSearchElement.detailedError);
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

  suite('AndroidContinuousRecognition', () => {
    let voiceSearchElement: ComposeboxVoiceSearchElement;
    let finalResult: string|null;
    let recordingStoppedTranscript: string|null;

    setup(async () => {
      loadTimeData.overrideValues({androidSpeechRecognition: true});
      await createComposeboxElement();
      voiceSearchElement = getVoiceSearchElement(composeboxElement);
      voiceSearchElement.androidContinuousRecognitionEnabled = true;
      voiceSearchElement.idleTimeout = 3000;
      voiceSearchElement.manualSubmitIdleTimeout = 10000;

      finalResult = null;
      recordingStoppedTranscript = null;
      voiceSearchElement.addEventListener(
          'voice-search-final-result', (e: Event) => {
            finalResult = (e as CustomEvent<string>).detail;
          });
      voiceSearchElement.addEventListener('recording-stopped', (e: Event) => {
        recordingStoppedTranscript = (e as CustomEvent<string>).detail;
      });

      getVoiceSearchButton(composeboxElement)!.click();
      await microtasksFinished();
      assertEquals(1, mockSpeechRecognition.startCount);
      mockSpeechRecognition.onaudiostart!(new Event('audiostart'));
    });

    // Simulates Android's recognizer reporting the transcript of its current
    // session.
    function sendResult(transcript: string) {
      const result = createResults(1);
      Object.assign(result.results[0]![0]!, {confidence: 0, transcript});
      mockSpeechRecognition.onresult!(result);
    }

    // Simulates Android's recognizer ending its session on its own.
    function endSession() {
      mockSpeechRecognition.voiceSearchInProgress = false;
      mockSpeechRecognition.onend!();
    }

    // Runs the callback of the latest timer that was set with `delayMs`.
    function fireTimer(delayMs: number) {
      const timerCall = [...windowProxy.getArgs('setTimeout')].reverse().find(
          (call: [unknown, number]) => call[1] === delayMs);
      assertTrue(!!timerCall);
      (timerCall[0] as Function)();
    }

    function getDisplayedTranscript(): string {
      const input =
          voiceSearchElement.shadowRoot.querySelector<HTMLElement>('#input');
      assertTrue(!!input);
      return input.textContent?.trim() ?? '';
    }

    test('restarts recognition when Android ends the session', async () => {
      sendResult('hello');
      endSession();
      await microtasksFinished();

      assertEquals(2, mockSpeechRecognition.startCount);
      assertTrue(mockSpeechRecognition.voiceSearchInProgress);
      assertEquals('hello', getDisplayedTranscript());
      assertEquals(null, finalResult);
      assertEquals(null, recordingStoppedTranscript);
    });

    test(
        'joins transcripts and stops recording on idle timeout in manual ' +
            'submit mode',
        async () => {
          sendResult('hello');
          endSession();
          sendResult('world');
          await microtasksFinished();
          assertEquals('hello world', getDisplayedTranscript());

          fireTimer(10000);
          await microtasksFinished();

          assertEquals('hello world', recordingStoppedTranscript);
          assertEquals(null, finalResult);
        });

    test(
        'joins transcripts and submits on idle timeout in auto submit mode',
        async () => {
          voiceSearchElement.autosubmitEnabled = true;
          sendResult('hello');
          endSession();
          sendResult('world');

          fireTimer(3000);
          await microtasksFinished();

          assertEquals('hello world', finalResult);
          assertEquals(null, recordingStoppedTranscript);
        });

    test(
        'does not add a space if the previous session ends with whitespace',
        async () => {
          sendResult('hello ');
          endSession();
          sendResult('world');
          await microtasksFinished();

          assertEquals('hello world', getDisplayedTranscript());
        });

    test('keeps the submit button enabled after a restart', async () => {
      voiceSearchElement.submitStopButtonsEnabled = true;
      sendResult('hello');
      endSession();
      // The restarted session has not recognized anything yet.
      sendResult('');
      await microtasksFinished();

      const submitButton =
          voiceSearchElement.shadowRoot.querySelector('#submitButton');
      assertTrue(!!submitButton);
      assertFalse(submitButton.hasAttribute('disabled'));
    });

    test('applies the query length limit to the whole transcript', () => {
      voiceSearchElement.queryLengthLimit = 10;
      sendResult('hello');
      endSession();
      // What earlier sessions recognized counts towards the limit too.
      sendResult('you');
      assertEquals(null, recordingStoppedTranscript);

      sendResult('world');
      assertEquals('hello world', recordingStoppedTranscript);
    });

    test('ignores no-speech and no-match errors', async () => {
      // Before any speech.
      mockSpeechRecognition.onerror!(new SpeechRecognitionErrorEvent(
          'error', {message: '', error: 'no-speech'}));
      endSession();
      assertEquals(2, mockSpeechRecognition.startCount);

      // After speech.
      sendResult('hello');
      mockSpeechRecognition.onnomatch!(new Event('nomatch'));
      endSession();
      await microtasksFinished();

      assertEquals(3, mockSpeechRecognition.startCount);
      assertEquals(null, voiceSearchElement.detailedError);
      assertEquals('hello', getDisplayedTranscript());
    });

    test('restarted sessions do not reset the idle timer', () => {
      sendResult('hello');
      endSession();
      windowProxy.resetResolver('setTimeout');

      mockSpeechRecognition.onaudiostart!(new Event('audiostart'));
      mockSpeechRecognition.onspeechstart!(new Event('speechstart'));
      assertEquals(0, windowProxy.getCallCount('setTimeout'));

      // Like on desktop, results still do.
      sendResult('world');
      assertTrue(windowProxy.getCallCount('setTimeout') > 0);
    });

    test(
        'ends with an error after too many restarts without results',
        async () => {
          sendResult('hello');
          for (let i = 0; i < 5; i++) {
            endSession();
          }
          assertEquals(6, mockSpeechRecognition.startCount);

          endSession();
          await microtasksFinished();

          assertEquals(6, mockSpeechRecognition.startCount);
          assertEquals(
              1,
              metrics.count(
                  'VoiceSearch.Errors.NTP_REALBOX',
                  VoiceSearchError.NO_SPEECH));
          // Neither submits nor stops recording with the transcript.
          assertEquals(null, finalResult);
          assertEquals(null, recordingStoppedTranscript);
        });

    test('ends with an error if recognition fails to restart', async () => {
      sendResult('hello');
      mockSpeechRecognition.start = () => {
        throw new DOMException('', 'InvalidStateError');
      };
      endSession();
      await microtasksFinished();

      assertEquals(VoiceSearchError.OTHER, voiceSearchElement.detailedError);
      assertEquals(null, finalResult);
      assertEquals(null, recordingStoppedTranscript);
    });

    test('does not restart recognition while the page is hidden', async () => {
      sendResult('hello');
      // Blink aborts recognition on Android when the page gets hidden.
      Object.defineProperty(
          document, 'hidden', {value: true, configurable: true});
      try {
        endSession();
      } finally {
        Reflect.deleteProperty(document, 'hidden');
      }
      await microtasksFinished();

      assertEquals(1, mockSpeechRecognition.startCount);
      assertEquals('hello', recordingStoppedTranscript);
      assertEquals(null, finalResult);
    });

    test(
        'does not restart recognition once voice search is removed',
        async () => {
          sendResult('hello');
          composeboxElement.remove();
          await microtasksFinished();

          assertEquals(1, mockSpeechRecognition.startCount);
          assertFalse(mockSpeechRecognition.voiceSearchInProgress);
        });

    test(
        'submits when Android ends the session if continuous recognition is ' +
            'disabled',
        async () => {
          voiceSearchElement.androidContinuousRecognitionEnabled = false;
          sendResult('hello');
          endSession();
          await microtasksFinished();

          assertEquals(1, mockSpeechRecognition.startCount);
          assertEquals('hello', finalResult);
        });
  });
});
