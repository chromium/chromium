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
import {WindowProxy} from 'chrome://resources/cr_components/composebox/window_proxy.js';
import type {AudioWaveElement} from 'chrome://resources/cr_components/search/audio_wave.js';
import type {RecordingWaveElement} from 'chrome://resources/cr_components/search/recording_wave.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeMediaQueryList} from 'chrome://webui-test/fake_media_query_list.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {$$, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {disableTransitionsRecursively, installMock, MockSpeechRecognition, mockSpeechRecognition} from './composebox_test_utils.js';


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

suite('ComposeboxVoiceSearchFlags', () => {
  let composeboxElement: ComposeboxElement;
  let handler: TestMock<PageHandlerRemote>;
  let searchboxHandler: TestMock<SearchboxPageHandlerRemote>;
  let windowProxy: TestMock<WindowProxy>;

  suiteSetup(() => {
    loadTimeData.overrideValues({
      composeboxShowZps: true,
      composeboxShowTypedSuggest: true,
    });
  });

  setup(async () => {
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

  test(
      'recording wave is hidden when not listening for composebox',
      async () => {
        loadTimeData.overrideValues({
          voiceSearchCoherenceComposeboxesEnabled: true,
        });
        await createComposeboxElement();
        composeboxElement.inVoiceSearchMode = false;
        await microtasksFinished();

        // SearchAnimatedGlow unconditionally exists
        const searchAnimatedGlow =
            composeboxElement.shadowRoot.querySelector('search-animated-glow');
        await microtasksFinished();

        const recordingWave: RecordingWaveElement|null =
            searchAnimatedGlow!.shadowRoot.querySelector('recording-wave');
        assertTrue(!!recordingWave);
        assertFalse(recordingWave.isListening);

        const audioWave =
            searchAnimatedGlow!.shadowRoot.querySelector('audio-wave');
        assertFalse(!!audioWave);

        const voiceSearchElement = getVoiceSearchElement(composeboxElement);
        assertTrue(!!voiceSearchElement, 'Voice search element should exist');

        const stopButton =
            voiceSearchElement.shadowRoot.querySelector('#stopButton');
        assertFalse(isVisible(stopButton), 'Stop button should not be shown');

        const submitButton =
            voiceSearchElement.shadowRoot.querySelector('#submitButton');
        assertFalse(
            isVisible(submitButton), 'Submit button should not be shown');
      });

  test('recording wave is rendered when listening for searchbox', async () => {
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
  });

  test(
      'recording wave is hidden when not listening for searchbox', async () => {
        loadTimeData.overrideValues({
          voiceSearchCoherenceComposeboxesEnabled: true,
        });
        await createComposeboxElement();

        composeboxElement.inVoiceSearchMode = false;
        await microtasksFinished();

        // SearchAnimatedGlow unconditionally exists
        const searchAnimatedGlow =
            composeboxElement.shadowRoot.querySelector('search-animated-glow');
        await microtasksFinished();

        const recordingWave: RecordingWaveElement|null =
            searchAnimatedGlow!.shadowRoot.querySelector('recording-wave');
        assertTrue(!!recordingWave);
        assertFalse(recordingWave.isListening);

        const audioWave =
            searchAnimatedGlow!.shadowRoot.querySelector('audio-wave');
        assertFalse(!!audioWave);
      });

  test('live transcription shows if enabled', async () => {
    const voiceSearchElement = getVoiceSearchElement(composeboxElement);
    voiceSearchElement.liveTranscriptEnabled = true;

    await microtasksFinished();

    const textArea = voiceSearchElement.shadowRoot.querySelector('#input');
    assertTrue(
        !!textArea, 'text area should exist when live transcript is enabled');
  });

  test('live transcription hides if not enabled', async () => {
    const voiceSearchElement = getVoiceSearchElement(composeboxElement);
    voiceSearchElement.liveTranscriptEnabled = false;
    await microtasksFinished();

    const textArea = voiceSearchElement.shadowRoot.querySelector('#input');
    assertFalse(
        !!textArea,
        'text area should not exist when live transcript is not enabled');
  });

  test('stop submit button hides if not enabled', async () => {
    loadTimeData.overrideValues({
      voiceSearchCoherenceComposeboxesEnabled: false,
    });
    await createComposeboxElement();
    composeboxElement.inVoiceSearchMode = true;
    const voiceSearchElement = getVoiceSearchElement(composeboxElement);
    await microtasksFinished();
    const stopButton =
        voiceSearchElement.shadowRoot.querySelector('#stopButton');
    assertFalse(!!stopButton, 'Stop button should not be shown');

    const submitButton =
        voiceSearchElement.shadowRoot.querySelector('#submitButton');
    assertFalse(!!submitButton, 'Submit button should not be shown');
  });

  test('stop submit button shows if enabled', async () => {
    loadTimeData.overrideValues({
      voiceSearchCoherenceComposeboxesEnabled: true,
    });
    await createComposeboxElement();
    composeboxElement.inVoiceSearchMode = true;
    const voiceSearchElement = getVoiceSearchElement(composeboxElement);
    await microtasksFinished();
    const stopButton =
        voiceSearchElement.shadowRoot.querySelector('#stopButton');
    assertTrue(!!stopButton, 'Stop button should be shown');

    const submitButton =
        voiceSearchElement.shadowRoot.querySelector('#submitButton');
    assertTrue(!!submitButton, 'Submit button should be shown');
  });

  test(
      'voice search container is empty without webkitSpeechRecognition API',
      async () => {
        // Temporarily remove API
        windowProxy.setResultFor('hasWebkitSpeechRecognition', false);
        await microtasksFinished();

        await createComposeboxElement();

        // Query the DOM directly instead of using the `getVoiceSearchElement`
        // helper, because the helper internally asserts that the element exists
        // (assertTrue), which would cause this test to fail prematurely.
        const voiceSearchElement = $$<ComposeboxVoiceSearchElement>(
            composeboxElement, 'cr-composebox-voice-search');
        assertFalse(!!voiceSearchElement);
      });

  test(
      'recovers from STARTED state missing audio and speech events',
      async () => {
        const voiceSearchButton = getVoiceSearchButton(composeboxElement);
        assertTrue(!!voiceSearchButton);
        voiceSearchButton.click();
        await microtasksFinished();

        const voiceSearchElement =
            getVoiceSearchElement(composeboxElement) as any;

        // Listen for speech-received event to prove onSpeechStart_ was manually
        // called.
        let speechReceivedFired = false;
        voiceSearchElement.addEventListener('speech-received', () => {
          speechReceivedFired = true;
        });

        // Construct a mock speech recognition result using the existing helper.
        const result = createResults(1);
        Object.assign(
            result.results[0]![0]!, {confidence: 1, transcript: 'test1'});
        Object.assign(result.results[0]!, {isFinal: false});

        // Trigger onresult directly while state is still STARTED.
        mockSpeechRecognition.onresult!(result);
        await microtasksFinished();

        // Assert: Fallback logic should manually trigger the missing speech
        // event.
        assertTrue(speechReceivedFired);
        // Assert: The result should be processed normally.
        assertEquals('test1', voiceSearchElement.transcript_);
      });

  test('recovers from AUDIO_RECEIVED state missing speech event', async () => {
    const voiceSearchButton = getVoiceSearchButton(composeboxElement);
    assertTrue(!!voiceSearchButton);
    voiceSearchButton.click();
    await microtasksFinished();

    const voiceSearchElement = getVoiceSearchElement(composeboxElement) as any;

    // Simulate audiostart event so the state becomes AUDIO_RECEIVED.
    mockSpeechRecognition.onaudiostart!(new Event('audiostart'));
    await microtasksFinished();

    let speechReceivedFired = false;
    voiceSearchElement.addEventListener('speech-received', () => {
      speechReceivedFired = true;
    });

    const result = createResults(1);
    Object.assign(result.results[0]![0]!, {confidence: 1, transcript: 'test2'});
    Object.assign(result.results[0]!, {isFinal: false});

    // Trigger onresult while state is AUDIO_RECEIVED.
    mockSpeechRecognition.onresult!(result);
    await microtasksFinished();

    // Assert: Fallback logic should manually trigger the missing speech
    // event.
    assertTrue(speechReceivedFired);
    assertEquals('test2', voiceSearchElement.transcript_);
  });

  test(
      'ignores late results in unexpected states like ERROR_RECEIVED',
      async () => {
        const voiceSearchButton = getVoiceSearchButton(composeboxElement);
        assertTrue(!!voiceSearchButton);
        voiceSearchButton.click();
        await microtasksFinished();

        const voiceSearchElement =
            getVoiceSearchElement(composeboxElement) as any;

        // Simulate a network error so the state becomes ERROR_RECEIVED.
        mockSpeechRecognition.onerror!(new SpeechRecognitionErrorEvent(
            'error', {message: '', error: 'network'}));
        await microtasksFinished();

        let transcriptUpdateFired = false;
        voiceSearchElement.addEventListener('transcript-update', () => {
          transcriptUpdateFired = true;
        });

        // Construct a late recognition result.
        const lateResult = createResults(1);
        Object.assign(
            lateResult.results[0]![0]!,
            {confidence: 1, transcript: 'late text'});
        Object.assign(lateResult.results[0]!, {isFinal: false});

        // Trigger onresult while state is already ERROR_RECEIVED.
        mockSpeechRecognition.onresult!(lateResult);
        await microtasksFinished();

        // Assert: The result should be completely ignored (default switch
        // case).
        assertFalse(transcriptUpdateFired);
        assertEquals('', voiceSearchElement.transcript_);
      });

  test('contextual tasks uses correct flag', async () => {
    loadTimeData.overrideValues({
      // These are not the flags themselves, but the loadtime values, so
      // the value derived from the main flag can be false here while
      // the value derived from the feature param can be true.
      voiceSearchCoherenceComposeboxesEnabled: false,
      voiceSearchCoherenceCobrowsingComposeboxEnabled: true,
    });
    await createComposeboxElement();

    composeboxElement.entrypointName = 'ContextualTasks';
    composeboxElement.inVoiceSearchMode = true;  // Render voice search.
    await microtasksFinished();

    const searchAnimated = composeboxElement.$.animatedSearchElement;
    const voiceSearchElement = getVoiceSearchElement(composeboxElement);
    assertTrue(
        searchAnimated.coloredTicTacVoiceAnimationEnabled,
        'Animation should be enabled when cobrowsing voice' +
            ' coherence is enabled');
    assertFalse(
        voiceSearchElement.liveTranscriptEnabled,
        'Live transcript should be disabled when cobrowsing' +
            ' voice coherence is enabled');
    assertTrue(
        voiceSearchElement.submitStopButtonsEnabled,
        'Stop submit buttons should be enabled when cobrowsing' +
            ' voice coherence is enabled');
  });

  test('omnibox uses correct flag', async () => {
    loadTimeData.overrideValues({
      voiceSearchCoherenceComposeboxesEnabled: false,
      voiceSearchCoherenceCobrowsingComposeboxEnabled: true,
    });
    await createComposeboxElement();

    composeboxElement.entrypointName = 'Omnibox';
    composeboxElement.inVoiceSearchMode = true;  // Render voice search.
    await microtasksFinished();

    const searchAnimated = composeboxElement.$.animatedSearchElement;
    const voiceSearchElement = getVoiceSearchElement(composeboxElement);
    assertFalse(
        searchAnimated.coloredTicTacVoiceAnimationEnabled,
        'Animation should be disabled for omnibox when' +
            ' only cobrowsing is enabled');
    assertTrue(
        voiceSearchElement.liveTranscriptEnabled,
        'Live transcription should be enabled for omnibox' +
            'when only cobrowsing is enabled');
    assertFalse(
        voiceSearchElement.submitStopButtonsEnabled,
        'Stop submit buttons should be disabled for omnibox' +
            'when only cobrowsing is enabled');
  });

  test('NTP uses correct flag', async () => {
    loadTimeData.overrideValues({
      voiceSearchCoherenceComposeboxesEnabled: false,
      voiceSearchCoherenceCobrowsingComposeboxEnabled: true,
    });
    await createComposeboxElement();

    composeboxElement.entrypointName = 'NTP';
    composeboxElement.inVoiceSearchMode = true;  // Render voice search.
    await microtasksFinished();

    const searchAnimated = composeboxElement.$.animatedSearchElement;
    const voiceSearchElement = getVoiceSearchElement(composeboxElement);
    assertFalse(
        searchAnimated.coloredTicTacVoiceAnimationEnabled,
        'Animation should be disabled for NTP when' +
            ' only cobrowsing is enabled');
    assertTrue(
        voiceSearchElement.liveTranscriptEnabled,
        'Live transcription should be enabled for NTP' +
            'when only cobrowsing is enabled');
    assertFalse(
        voiceSearchElement.submitStopButtonsEnabled,
        'Stop submit buttons should be disabled for NTP' +
            'when only cobrowsing is enabled');
  });

  test(
      'Enabled voice search coherence results in correct' +
          ' parameters being passed',
      async () => {
        loadTimeData.overrideValues({
          voiceSearchCoherenceComposeboxesEnabled: true,
        });
        await createComposeboxElement();

        composeboxElement.entrypointName = 'Omnibox';
        composeboxElement.inVoiceSearchMode = true;  // Render voice search.
        await microtasksFinished();

        const searchAnimated = composeboxElement.$.animatedSearchElement;
        const voiceSearchElement = getVoiceSearchElement(composeboxElement);
        assertTrue(
            searchAnimated.coloredTicTacVoiceAnimationEnabled,
            'Animation should be enabled when composeboxes' +
                'voice coherence is enabled');
        assertFalse(
            voiceSearchElement.liveTranscriptEnabled,
            'Live transcript should be disabled when composeboxes' +
                'voice coherence is enabled');
        assertTrue(
            voiceSearchElement.submitStopButtonsEnabled,
            'Stop submit buttons should be enabled when composeboxes' +
                ' voice coherence is enabled');
      });

  test(
      'Disabled voice search coherence results in' +
          ' correct parameters being passed',
      async () => {
        loadTimeData.overrideValues({
          voiceSearchCoherenceComposeboxesEnabled: false,
        });
        await createComposeboxElement();

        composeboxElement.entrypointName = 'Omnibox';
        composeboxElement.inVoiceSearchMode = true;  // Render voice search.
        await microtasksFinished();

        const searchAnimated = composeboxElement.$.animatedSearchElement;
        const voiceSearchElement = getVoiceSearchElement(composeboxElement);
        assertFalse(
            searchAnimated.coloredTicTacVoiceAnimationEnabled,
            'Animation should be disabled when composeboxes' +
                ' voice coherence is disabled');
        assertTrue(
            voiceSearchElement.liveTranscriptEnabled,
            'Live transcript should be enabled when composeboxes' +
                'voice coherence is disabled');
        assertFalse(
            voiceSearchElement.submitStopButtonsEnabled,
            'Stop submit buttons should be disabled when composeboxes' +
                'voice coherence is disabled');
      });

  test(
      'onResult_ force-submits when interim result exceeds length limit',
      async () => {
        const voiceSearchButton = getVoiceSearchButton(composeboxElement);
        assertTrue(!!voiceSearchButton);
        voiceSearchButton.click();
        await microtasksFinished();

        const voiceSearchElement = getVoiceSearchElement(composeboxElement);
        // Set at 120 words.
        voiceSearchElement.queryLengthLimit = 120;  // non-default limit.
        voiceSearchElement.autosubmitEnabled = true;

        // Listen for the final result event to verify if it was
        // force-submitted.
        let finalResultFired = false;
        let submittedResult = '';
        voiceSearchElement.addEventListener(
            'voice-search-final-result', (e: any) => {
              finalResultFired = true;
              submittedResult = e.detail;
            });

        // Construct a long string exceeding the set 120 character limit.
        const longTranscript = 'a'.repeat(121);
        const result = createResults(1);

        // Set confidence to 0 to ensure it is treated as an interim result.
        Object.assign(
            result.results[0]![0]!,
            {confidence: 0, transcript: longTranscript});
        Object.assign(result.results[0]!, {isFinal: false});

        // Simulate receiving this long interim result.
        mockSpeechRecognition.onresult!(result);
        await microtasksFinished();

        // Assert: The system should force-submit it as a final result due to
        // the length limit.
        assertTrue(finalResultFired);
        assertEquals(longTranscript, submittedResult);
      });
});
