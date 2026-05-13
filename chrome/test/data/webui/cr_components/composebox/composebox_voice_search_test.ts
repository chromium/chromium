// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/strings.m.js';
import 'chrome://resources/cr_components/composebox/composebox.js';
import 'chrome://resources/cr_components/composebox/composebox_voice_search.js';

import type {ComposeboxElement} from 'chrome://resources/cr_components/composebox/composebox.js';
import {PageCallbackRouter, PageHandlerRemote} from 'chrome://resources/cr_components/composebox/composebox.mojom-webui.js';
import {ComposeboxProxyImpl, createAutocompleteMatch} from 'chrome://resources/cr_components/composebox/composebox_proxy.js';
import type {ComposeboxVoiceSearchElement} from 'chrome://resources/cr_components/composebox/composebox_voice_search.js';
import {VoiceSearchAction, VoiceSearchError} from 'chrome://resources/cr_components/composebox/composebox_voice_search.js';
import {WindowProxy} from 'chrome://resources/cr_components/composebox/window_proxy.js';
import type {RecordingWaveElement} from 'chrome://resources/cr_components/search/recording_wave.js';
import type {AudioWaveElement} from 'chrome://resources/cr_components/search/audio_wave.js';
import {GlowAnimationState} from 'chrome://resources/cr_components/search/constants.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {$$, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

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
  onaudiostart: ((this: MockSpeechRecognition, ev: Event) => void)|null = null;
  onspeechstart: ((this: MockSpeechRecognition, ev: Event) => void)|null = null;
  onnomatch: ((this: MockSpeechRecognition, ev: Event) => void)|null = null;
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

type MockComposeboxVoiceSearch = Omit<
    ComposeboxVoiceSearchElement,
    'state_'|'voiceRecognition_'|'onFinalResult_'|'onCloseClick_'|'onEnd_'|
    'onTryAgainClick_'|'onLinkClick_'>&{
  state_: number,
  metricSource_: string,
  voiceRecognition_: MockSpeechRecognition,
  onFinalResult_: (result: string) => void,
  onCloseClick_: () => void,
  onEnd_: () => void,
  onTryAgainClick_: (e: Event) => void,
  onLinkClick_: (e: Event) => void,
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
      composeboxShowZps: true,
      composeboxShowTypedSuggest: true,
      composeboxSmartTabSharingVisible: false,
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
    handler.setResultMapperFor(
        'getSmartTabSharingActive', () => Promise.resolve({active: false}));
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

    loadTimeData.overrideValues({
      voiceSearchCoherenceComposeboxesEnabled: false,
      voiceSearchCoherenceAnySearchboxExperimentEnabled: false,
    });

    window.webkitSpeechRecognition =
        MockSpeechRecognition as unknown as typeof SpeechRecognition;

    composeboxElement = document.createElement('cr-composebox');
    composeboxElement.showVoiceSearch = true;
    document.body.appendChild(composeboxElement);
  });

  async function createComposeboxElement() {
    composeboxElement = document.createElement('cr-composebox');
    composeboxElement.showVoiceSearch = true;
    document.body.appendChild(composeboxElement);
    await microtasksFinished();
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

  async function openVoiceSearchUI() {
    const hidePromise =
        getTransitionEndPromise(composeboxElement.$.composebox, 'opacity');
    getVoiceSearchButton(composeboxElement)!.click();
    await microtasksFinished();
    await hidePromise;

    const voiceSearchElement = getVoiceSearchElement(composeboxElement);
    windowProxy.resetResolver('setTimeout');

    return voiceSearchElement;
  }

  test('verifies idle timeout is 1500ms', async () => {
    // Open the UI. This calls start(), but openVoiceSearchUI()
    // swallows/resets the setTimeout tracker at the end.
    await openVoiceSearchUI();

    // Trigger an audio event to force the timer to reset.
    // This will generate a fresh setTimeout call outside of the
    // openVoiceSearchUI() function.
    mockSpeechRecognition.onaudiostart!(new Event('audiostart'));

    const [, timeoutMs] = await windowProxy.whenCalled('setTimeout');

    // Ensure it matches Google3.
    assertEquals(1500, timeoutMs);
  });

  test('ABORTED error bypasses timer management', async () => {
    await openVoiceSearchUI();
    windowProxy.reset();

    mockSpeechRecognition.onerror!
        ({error: 'aborted'} as SpeechRecognitionErrorEvent);
    await microtasksFinished();

    // No timers should be called when 'aborted' is received.
    assertEquals(0, windowProxy.getCallCount('clearTimeout'));
    assertEquals(0, windowProxy.getCallCount('setTimeout'));
  });

  test('clears active timers when user manually closes the UI', async () => {
    const voiceSearchElement = await openVoiceSearchUI();
    windowProxy.reset();  // Clear trackers from the start() phase

    // Simulate user explicitly closing the interface
    const mockVoiceSearch =
        voiceSearchElement as unknown as MockComposeboxVoiceSearch;
    mockVoiceSearch.onCloseClick_();
    await microtasksFinished();

    // Assert that clearTimeout is called twice during the teardown sequence
    // (once by the abort()->onError cascade, and once by resetState_)
    assertEquals(
        2, windowProxy.getCallCount('clearTimeout'),
        'clearTimeout must be called on close to prevent background execution');

    // No timers should be created during cleanup.
    assertEquals(
        0, windowProxy.getCallCount('setTimeout'),
        'No new timers should be created during cleanup');
  });

  test('idle timer resets dynamically during continuous speech', async () => {
    await openVoiceSearchUI();

    windowProxy.resetResolver('setTimeout');
    windowProxy.reset();

    mockSpeechRecognition.onaudiostart!(new Event('audiostart'));
    await microtasksFinished();

    mockSpeechRecognition.onspeechstart!(new Event('speechstart'));
    await microtasksFinished();

    const result = createResults(1);
    Object.defineProperty(
        result.results[0]![0]!, 'transcript',
        {value: 'testing', writable: true, configurable: true});
    mockSpeechRecognition.onresult!(result);
    await microtasksFinished();

    // Expect windowProxy to react to these events: start(), onAudioStart_(),
    // onSpeechStart_(), onResult_(), to reset the previous idle timer and start
    // a fresh one.
    assertTrue(
        windowProxy.getCallCount('clearTimeout') >= 3,
        'clearTimeout should be called to destroy the previous idle timer');
    assertTrue(
        windowProxy.getCallCount('setTimeout') >= 3,
        'setTimeout should be called to start a fresh idle timer');
  });

  test('hides stop and submit buttons when error scrim is shown', async () => {
    loadTimeData.overrideValues({
      voiceSearchCoherenceComposeboxesEnabled: true,
    });
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    composeboxElement = document.createElement('cr-composebox');
    composeboxElement.showVoiceSearch = true;
    document.body.appendChild(composeboxElement);
    await microtasksFinished();

    const voiceSearchElement = await openVoiceSearchUI();

    mockSpeechRecognition.onerror!
        ({error: 'network'} as SpeechRecognitionErrorEvent);
    await microtasksFinished();
    await voiceSearchElement.updateComplete;

    const errorContainer = $$(voiceSearchElement, '#error-container');
    const bottomActions =
        voiceSearchElement.shadowRoot.querySelector<HTMLElement>(
            '#bottomActions');

    assertTrue(!!errorContainer, 'Error container should exist');
    assertFalse(errorContainer.hidden, 'Error container should be visible');
    assertTrue(!!bottomActions, 'Bottom actions container should exist');
    assertFalse(isVisible(bottomActions), 'Bottom actions should be hidden');
  });

  test(
      'does not hide cancel button in voice search if flag is false',
      async () => {
        loadTimeData.overrideValues({
          voiceSearchCoherenceComposeboxesEnabled: false,
        });
        document.body.innerHTML = window.trustedTypes!.emptyHTML;
        composeboxElement = document.createElement('cr-composebox');
        composeboxElement.showVoiceSearch = true;
        document.body.appendChild(composeboxElement);
        await microtasksFinished();

        const voiceSearchElement = await openVoiceSearchUI();
        let closeButton =
            voiceSearchElement.shadowRoot.querySelector('#closeButton');
        assertTrue(!!closeButton, 'close button should be shown');

        mockSpeechRecognition.onerror!
            ({error: 'network'} as SpeechRecognitionErrorEvent);
        await microtasksFinished();
        await voiceSearchElement.updateComplete;

        const errorContainer = $$(voiceSearchElement, '#error-container');
        assertTrue(!!errorContainer, 'Error container should exist');
        assertFalse(errorContainer.hidden, 'Error container should be visible');

        closeButton =
            voiceSearchElement.shadowRoot.querySelector('#closeButton');
        assertTrue(!!closeButton, 'close button should be shown');
      });

  test(
      'hides cancel button when there is no error in voice search',
      async () => {
        loadTimeData.overrideValues({
          voiceSearchCoherenceComposeboxesEnabled: true,
        });
        document.body.innerHTML = window.trustedTypes!.emptyHTML;
        composeboxElement = document.createElement('cr-composebox');
        composeboxElement.showVoiceSearch = true;
        document.body.appendChild(composeboxElement);
        await microtasksFinished();

        const voiceSearchElement = await openVoiceSearchUI();
        const closeButton =
            voiceSearchElement.shadowRoot.querySelector('#closeButton');
        assertFalse(!!closeButton, 'close button should be hidden');
      });

  test('shows cancel button when error scrim is shown', async () => {
    loadTimeData.overrideValues({
      voiceSearchCoherenceComposeboxesEnabled: true,
    });
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    composeboxElement = document.createElement('cr-composebox');
    composeboxElement.showVoiceSearch = true;
    document.body.appendChild(composeboxElement);
    await microtasksFinished();

    const voiceSearchElement = await openVoiceSearchUI();

    mockSpeechRecognition.onerror!
        ({error: 'network'} as SpeechRecognitionErrorEvent);
    await microtasksFinished();
    await voiceSearchElement.updateComplete;

    const errorContainer = $$(voiceSearchElement, '#error-container');
    assertTrue(!!errorContainer, 'Error container should exist');
    assertFalse(errorContainer.hidden, 'Error container should be visible');

    const closeButton =
        voiceSearchElement.shadowRoot.querySelector('#closeButton');
    assertTrue(!!closeButton, 'close button should be shown during error');
  });

  test(
      'NO_MATCH error auto-closes immediately when hasErrorTimer is false',
      async () => {
        const voiceSearchElement = await openVoiceSearchUI();
        // Simulate Composebox behavior.
        voiceSearchElement.hasErrorTimer = false;

        let cancelEventFired = false;
        voiceSearchElement.addEventListener('voice-search-cancel', () => {
          cancelEventFired = true;
        });

        voiceSearchElement['voiceRecognition_'].onnomatch!
            (new Event('nomatch'));
        await microtasksFinished();

        // Verify immediate closure with no error message and no timer.
        assertTrue(cancelEventFired);
        assertEquals('', voiceSearchElement['errorMessage_']);
        assertEquals(0, windowProxy.getCallCount('setTimeout'));
        assertEquals(
            1,
            metrics.count(
                'VoiceSearch.Action.NTP_REALBOX',
                VoiceSearchAction.ERROR_CANCELING));
        // Clean up internal state to prevent leaking into the next test.
        voiceSearchElement['voiceModeEndCleanup_']();
        await microtasksFinished();
      });

  test(
      'Other errors keep UI open permanently when hasErrorTimer is false',
      async () => {
        const voiceSearchElement = await openVoiceSearchUI();
        // Simulate Composebox behavior.
        voiceSearchElement.hasErrorTimer = false;

        mockSpeechRecognition.onerror!
            ({error: 'network'} as SpeechRecognitionErrorEvent);
        await microtasksFinished();
        await composeboxElement.updateComplete;
        await voiceSearchElement.updateComplete;

        const errorContainer = $$(voiceSearchElement, '#error-container');
        const inputElement = $$(voiceSearchElement, '#input');

        // Verify the error UI remains open permanently with the correct text.
        assertTrue(!!errorContainer);
        assertFalse(errorContainer.hidden);
        assertTrue(inputElement!.hidden);
        assertEquals(
            loadTimeData.getString('networkError'),
            voiceSearchElement['errorMessage_']);

        assertStyle(composeboxElement.$.composebox, 'opacity', '0');
        assertStyle(voiceSearchElement, 'display', 'inline');

        assertEquals(
            1,
            metrics.count(
                'VoiceSearch.Action.NTP_REALBOX',
                VoiceSearchAction.ERROR_NON_CANCELING));
        // Clean up internal state to prevent leaking into the next test.
        voiceSearchElement['voiceModeEndCleanup_']();
        await microtasksFinished();
      });

  test(
      'idle timeout triggers NO_SPEECH and auto-closes instantly in Composebox',
      async () => {
        const hidePromise =
            getTransitionEndPromise(composeboxElement.$.composebox, 'opacity');
        const voiceSearchButton = getVoiceSearchButton(composeboxElement);
        voiceSearchButton!.click();
        await microtasksFinished();
        await hidePromise;

        const voiceSearchElement =
            getVoiceSearchElement(composeboxElement) as any;
        // Simulate Composebox behavior where timer is disabled.
        voiceSearchElement.hasErrorTimer = false;

        let cancelEventFired = false;
        voiceSearchElement.addEventListener('voice-search-cancel', () => {
          cancelEventFired = true;
        });

        // Intercept the 1.5s idle timer triggered during start().
        const [callback] = await windowProxy.whenCalled('setTimeout');
        // Reset resolver to verify no extra timers are created afterwards.
        windowProxy.resetResolver('setTimeout');
        callback();

        await microtasksFinished();
        await voiceSearchElement.updateComplete;

        // Assert: Component state is cleared due to instant resetState_().
        assertEquals(null, voiceSearchElement.detailedError_);
        // Assert: NO_SPEECH error was successfully recorded in metrics.
        assertEquals(
            1,
            metrics.count(
                'VoiceSearch.Errors.NTP_REALBOX', VoiceSearchError.NO_SPEECH));

        // Assert: UI closes instantly with no error message and no new timers.
        assertTrue(cancelEventFired);
        assertEquals('', voiceSearchElement['errorMessage_']);
        assertEquals(0, windowProxy.getCallCount('setTimeout'));

        // Assert: The action is logged as ERROR_CANCELING.
        assertEquals(
            1,
            metrics.count(
                'VoiceSearch.Action.NTP_REALBOX',
                VoiceSearchAction.ERROR_CANCELING));

        // Clean up internal state to prevent leaking into the next test.
        voiceSearchElement['voiceModeEndCleanup_']();
        await microtasksFinished();
      });

  test(
      'NO_MATCH error auto-closes after 24s when hasErrorTimer is true',
      async () => {
        const voiceSearchElement = await openVoiceSearchUI();
        // Simulate NTP searchbox behavior.
        voiceSearchElement.hasErrorTimer = true;

        let cancelEventFired = false;
        voiceSearchElement.addEventListener('voice-search-cancel', () => {
          cancelEventFired = true;
        });

        mockSpeechRecognition.onnomatch!(new Event('nomatch'));
        await microtasksFinished();

        // Verify the timer is exactly 24 seconds.
        const [callback, timeoutMs] =
            await windowProxy.whenCalled('setTimeout');
        assertEquals(24000, timeoutMs);

        // Simulate the timeout passing.
        callback();
        await microtasksFinished();

        assertTrue(cancelEventFired);
        assertEquals(null, voiceSearchElement.detailedError_);
        assertEquals(
            1,
            metrics.count(
                'VoiceSearch.Action.NTP_REALBOX',
                VoiceSearchAction.ERROR_CANCELING));
        // Clean up internal state to prevent leaking into the next test.
        voiceSearchElement['voiceModeEndCleanup_']();
        await microtasksFinished();
      });

  test(
      'Other errors auto-close after 9s when hasErrorTimer is true',
      async () => {
        const voiceSearchElement = await openVoiceSearchUI();
        // Simulate NTP searchbox behavior.
        voiceSearchElement.hasErrorTimer = true;

        let cancelEventFired = false;
        voiceSearchElement.addEventListener('voice-search-cancel', () => {
          cancelEventFired = true;
        });

        mockSpeechRecognition.onerror!
            ({error: 'network'} as SpeechRecognitionErrorEvent);
        await microtasksFinished();

        // Verify the timer is exactly 9 seconds.
        const [callback, timeoutMs] =
            await windowProxy.whenCalled('setTimeout');
        assertEquals(9000, timeoutMs);

        // Simulate the timeout passing.
        callback();
        await microtasksFinished();

        assertTrue(cancelEventFired);
        assertEquals(null, voiceSearchElement.detailedError_);
        assertEquals(
            1,
            metrics.count(
                'VoiceSearch.Action.NTP_REALBOX',
                VoiceSearchAction.ERROR_CANCELING));
        // Clean up internal state to prevent leaking into the next test.
        voiceSearchElement['voiceModeEndCleanup_']();
        await microtasksFinished();
      });

  test('voice search button does not show when disabled', async () => {
    composeboxElement = document.createElement('cr-composebox');
    composeboxElement.showVoiceSearch = false;
    document.body.appendChild(composeboxElement);
    await microtasksFinished();

    const voiceSearchButton = getVoiceSearchButton(composeboxElement);
    assertFalse(!!voiceSearchButton);
  });

  test('voice search button shows when enabled', () => {
    const voiceSearchButton = getVoiceSearchButton(composeboxElement);
    assertTrue(!!voiceSearchButton);
  });

  test(
      'stop and submit buttons show when coherence flag is enabled',
      async () => {
        // Enable flag and recreate element to apply new loadTimeData values.
        loadTimeData.overrideValues({
          voiceSearchCoherenceComposeboxesEnabled: true,
        });
        document.body.innerHTML = window.trustedTypes!.emptyHTML;
        composeboxElement = document.createElement('cr-composebox');
        composeboxElement.showVoiceSearch = true;
        document.body.appendChild(composeboxElement);
        await microtasksFinished();
        const voiceSearchButton = getVoiceSearchButton(composeboxElement);
        assertTrue(!!voiceSearchButton, 'Mic button should exist');
        voiceSearchButton.click();
        await microtasksFinished();

        const voiceSearchElement = getVoiceSearchElement(composeboxElement);

        const stopButton =
            voiceSearchElement.shadowRoot.querySelector('#stopButton');
        const submitButton =
            voiceSearchElement.shadowRoot.querySelector('#submitButton');

        assertTrue(
            isVisible(stopButton),
            'Stop button should be visible when flag is enabled');
        assertTrue(
            (!!submitButton),
            'Submit button should exist when flag is enabled');
      });

  test(
      'stop and submit buttons hide when coherence flag is disabled',
      async () => {
        // Disable flag and recreate element to apply new loadTimeData values.
        loadTimeData.overrideValues({
          voiceSearchCoherenceComposeboxesEnabled: false,
        });
        document.body.innerHTML = window.trustedTypes!.emptyHTML;
        composeboxElement = document.createElement('cr-composebox');
        composeboxElement.showVoiceSearch = true;
        document.body.appendChild(composeboxElement);
        await microtasksFinished();

        const voiceSearchElement = getVoiceSearchElement(composeboxElement);

        const stopButton =
            voiceSearchElement.shadowRoot.querySelector('#stopButton');
        const submitButton =
            voiceSearchElement.shadowRoot.querySelector('#submitButton');

        assertFalse(
            isVisible(stopButton),
            'Stop button should be hidden when flag is disabled');
        assertFalse(
            isVisible(submitButton),
            'Submit button should be hidden when flag is disabled');
      });

  test(
      'Records STOP_BUTTON_CLICKED action and fires event on stop click',
      async () => {
        loadTimeData.overrideValues({
          voiceSearchCoherenceComposeboxesEnabled: true,
        });

        document.body.innerHTML = window.trustedTypes!.emptyHTML;
        composeboxElement = document.createElement('cr-composebox');
        composeboxElement.showVoiceSearch = true;
        document.body.appendChild(composeboxElement);
        await microtasksFinished();

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
        voiceSearchElement.addEventListener('recording-stopped', (e: Event) => {
          firedTranscript = (e as CustomEvent<string>).detail;
        });

        // Simulate a user clicking the Stop button.
        const stopButton =
            voiceSearchElement.shadowRoot.querySelector<HTMLElement>(
                '#stopButton');
        assertTrue(!!stopButton);
        stopButton.click();
        await microtasksFinished();

        // Verify the emitted transcript is cleanly concatenated and trimmed.
        assertEquals('hello world', firedTranscript);

        // Verify that the voice search engine has successfully stopped.
        assertFalse(mockSpeechRecognition.voiceSearchInProgress);

        // Verify that the STOP_BUTTON_CLICKED metric was recorded.
        assertEquals(
            1,
            metrics.count(
                'VoiceSearch.Action.NTP_REALBOX',
                VoiceSearchAction.STOP_BUTTON_CLICKED));

        // Clean up internal state.
        voiceSearchElement['voiceModeEndCleanup_']();
        await microtasksFinished();
      });

  test('Emits clean transcript without duplicates on stop click', async () => {
    // Enable flag and recreate component.
    loadTimeData.overrideValues({
      voiceSearchCoherenceComposeboxesEnabled: true,
    });
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    composeboxElement = document.createElement('cr-composebox');
    composeboxElement.showVoiceSearch = true;
    document.body.appendChild(composeboxElement);
    await microtasksFinished();

    // Open the voice search UI.
    const voiceSearchButton = getVoiceSearchButton(composeboxElement);
    assertTrue(!!voiceSearchButton);
    voiceSearchButton.click();
    await microtasksFinished();

    const voiceSearchElement = getVoiceSearchElement(composeboxElement);
    const mockVoiceSearch =
        voiceSearchElement as unknown as MockComposeboxVoiceSearch;

    // Simulate first speech recognition event.
    const firstResult = createResults(1);
    Object.assign(
        firstResult.results[0]![0]!, {confidence: 1, transcript: 'hello'});
    mockVoiceSearch.voiceRecognition_.onresult!(firstResult);
    await microtasksFinished();

    // Simulate second speech recognition event (interim).
    const secondResult = createResults(2);
    Object.assign(
        secondResult.results[0]![0]!, {confidence: 1, transcript: 'hello'});
    Object.assign(
        secondResult.results[1]![0]!, {confidence: 0, transcript: ' world'});
    mockVoiceSearch.voiceRecognition_.onresult!(secondResult);
    await microtasksFinished();

    // Listen for the emitted transcript.
    let firedTranscript = '';
    voiceSearchElement.addEventListener('recording-stopped', (e: Event) => {
      firedTranscript = (e as CustomEvent<string>).detail;
    });

    // Click stop button.
    const stopButton =
        voiceSearchElement.shadowRoot.querySelector<HTMLElement>(
            '#stopButton');
    assertTrue(isVisible(stopButton), 'Stop button should be visible');
    stopButton!.click();
    await microtasksFinished();

    // Verify transcript has no duplicate text (e.g. 'hellohello world').
    assertEquals('hello world', firedTranscript);

    // Cleanup.
    mockVoiceSearch.state_ = -1;
    mockVoiceSearch.voiceRecognition_.abort();
    await microtasksFinished();
  });

  test('Submits the voice transcript accurately after stop click', async () => {
    const voiceTranscript = 'voice query';
    searchboxHandler.setResultMapperFor('queryAutocomplete', () => {
      return Promise.resolve({
        result: {
          input: voiceTranscript,
          matches: [
            createAutocompleteMatch({
              contents: voiceTranscript,
              fillIntoEdit: voiceTranscript,
              allowedToBeDefaultMatch: true,
              destinationUrl: 'https://fake.com',
            }),
          ],
          suggestionGroupsMap: {},
          smartComposeInlineHint: '',
          sequenceId: 0,
        },
      });
    });

    loadTimeData.overrideValues({
      voiceSearchCoherenceComposeboxesEnabled: true,
    });
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    composeboxElement = document.createElement('cr-composebox');
    document.body.appendChild(composeboxElement);
    await microtasksFinished();

    const voiceSearchButton = getVoiceSearchButton(composeboxElement);
    assertTrue(!!voiceSearchButton);
    voiceSearchButton.click();
    await microtasksFinished();

    const voiceSearchElement = getVoiceSearchElement(composeboxElement);

    const result = createResults(1);
    Object.assign(
        result.results[0]![0]!, {confidence: 1, transcript: voiceTranscript});
    mockSpeechRecognition.onresult!(result);
    await microtasksFinished();

    searchboxHandler.resetResolver('queryAutocomplete');

    const stopButton =
        voiceSearchElement.shadowRoot.querySelector<HTMLElement>('#stopButton');
    assertTrue(!!stopButton);
    stopButton.click();

    await searchboxHandler.whenCalled('queryAutocomplete');
    await microtasksFinished();

    searchboxHandler.resetResolver('submitQuery');
    const mainSubmitButton =
        composeboxElement.shadowRoot.querySelector<HTMLElement>(
            'cr-composebox-submit');
    assertTrue(!!mainSubmitButton);

    mainSubmitButton.dispatchEvent(
        new CustomEvent('submit-focusin', {bubbles: true, composed: true}));
    await microtasksFinished();

    mainSubmitButton.dispatchEvent(
        new CustomEvent('submit-click', {bubbles: true, composed: true}));
    await microtasksFinished();

    assertEquals(1, searchboxHandler.getCallCount('submitQuery'));
    const submitArgs = await searchboxHandler.whenCalled('submitQuery');
    assertEquals(
        voiceTranscript, submitArgs[0],
        'The submitted query must match the voice transcript');
  });

  test(
      'Queries autocomplete to update suggestions after stop click',
      async () => {
        // Reset handler calls to ensure a clean slate.
        searchboxHandler.resetResolver('queryAutocomplete');

        // Open voice search.
        const voiceSearchButton = getVoiceSearchButton(composeboxElement);
        assertTrue(!!voiceSearchButton);
        voiceSearchButton.click();
        await microtasksFinished();

        const voiceSearchElement = getVoiceSearchElement(composeboxElement);

        // Simulate speech recognition result.
        const result = createResults(1);
        Object.assign(
            result.results[0]![0]!,
            {confidence: 1, transcript: 'refresh suggestions'});
        mockSpeechRecognition.onresult!(result);
        await microtasksFinished();

        // Click the stop button.
        const stopButton =
            voiceSearchElement.shadowRoot.querySelector<HTMLElement>(
                '#stopButton');
        assertTrue(!!stopButton, 'Stop button should exist');
        stopButton.click();
        await microtasksFinished();

        // Verify queryAutocomplete was explicitly called to update suggestions.
        assertEquals(1, searchboxHandler.getCallCount('queryAutocomplete'));

        const queryArgs =
            await searchboxHandler.whenCalled('queryAutocomplete');
        assertEquals('refresh suggestions', queryArgs[0]);
        assertFalse(queryArgs[1]);  // verify clearMatches is false
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
          voiceSearchCoherenceComposeboxesEnabled: true,
        });

        document.body.innerHTML = window.trustedTypes!.emptyHTML;
        composeboxElement = document.createElement('cr-composebox');
        composeboxElement.showVoiceSearch = true;
        document.body.appendChild(composeboxElement);
        await microtasksFinished();

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

  test('on result updates the searchbox input', async () => {
    const voiceSearchButton = getVoiceSearchButton(composeboxElement);
    voiceSearchButton!.click();
    await microtasksFinished();

    const voiceSearchElement = getVoiceSearchElement(composeboxElement);

    const result = createResults(2);
    Object.assign(result.results[0]![0]!, {transcript: 'hello'});
    Object.assign(result.results[1]![0]!, {transcript: 'world'});

    // Act.
    mockSpeechRecognition.onresult!(result);
    await microtasksFinished();
    await voiceSearchElement.updateComplete;

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

    // Act.
    mockSpeechRecognition.onresult!(result2);
    await microtasksFinished();
    await voiceSearchElement.updateComplete;
    // Speech recognition overrides existing composebox input.
    assertEquals('hellogoodbye', voiceSearchInput.value);
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
    composeboxElement.showVoiceSearch = true;
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
      'on error keeps voice search open and shows error container for all errors',
      async () => {
        const hidePromise =
            getTransitionEndPromise(composeboxElement.$.composebox, 'opacity');
        const voiceSearchButton = getVoiceSearchButton(composeboxElement);
        voiceSearchButton!.click();
        await microtasksFinished();
        await hidePromise;

        // Simulate a generic error, e.g., network error.
        mockSpeechRecognition.onerror!
            ({error: 'network'} as SpeechRecognitionErrorEvent);
        await microtasksFinished();
        await composeboxElement.updateComplete;
        await getVoiceSearchElement(composeboxElement).updateComplete;

        const voiceSearchElement = getVoiceSearchElement(composeboxElement);
        const errorContainer = $$(voiceSearchElement, '#error-container');
        const inputElement = $$(voiceSearchElement, '#input');

        // Assert: The error container should be visible for ALL errors now.
        assertTrue(!!errorContainer);
        assertFalse(errorContainer.hidden);
        assertTrue(inputElement!.hidden);

        // Assert: The UI should remain open (not display: none).
        assertStyle(composeboxElement.$.composebox, 'opacity', '0');
        assertStyle(voiceSearchElement, 'display', 'inline');

        // Assert: The error message is populated directly from loadTimeData.
        assertEquals(
            loadTimeData.getString('networkError'),
            (voiceSearchElement as any).errorMessage_);
      });

  test('onEnd_ triggers AUDIO_CAPTURE error if state is STARTED', async () => {
    const voiceSearchButton = getVoiceSearchButton(composeboxElement);
    voiceSearchButton!.click();
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
    voiceSearchButton!.click();
    await microtasksFinished();

    // Simulate receiving a system ABORTED error.
    mockSpeechRecognition.onerror!
        ({error: 'aborted'} as SpeechRecognitionErrorEvent);
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
    await mockComposeboxElement.updateComplete;

    // SearchAnimatedGlow unconditionally exists
    const searchAnimatedGlow =
        mockComposeboxElement.shadowRoot.querySelector('search-animated-glow');
    await searchAnimatedGlow!.updateComplete;

    const audioWave: AudioWaveElement|null =
        searchAnimatedGlow!.shadowRoot.querySelector('audio-wave');
    assertTrue(!!audioWave, 'Audio wave should be shown');
    const recordingWave: RecordingWaveElement|null =
        searchAnimatedGlow!.shadowRoot.querySelector('recording-wave');
    assertFalse(!!recordingWave, 'Recording wave should not be shown');

    mockComposeboxElement.transcript = 'foo';
    await mockComposeboxElement.updateComplete;
    await searchAnimatedGlow!.updateComplete;
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
    await searchAnimatedGlow!.updateComplete;
    const audioWave: AudioWaveElement|null =
        searchAnimatedGlow!.shadowRoot.querySelector('audio-wave');
    assertFalse(!!audioWave, 'Audio wave should not be shown');

    const recordingWave: RecordingWaveElement|null =
        searchAnimatedGlow!.shadowRoot.querySelector('recording-wave');
    assertFalse(!!recordingWave, 'Recording wave should not be shown');
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
    await searchAnimatedGlow!.updateComplete;
    const recordingWave: RecordingWaveElement|null =
        searchAnimatedGlow!.shadowRoot.querySelector('recording-wave');
    assertTrue(!!recordingWave, 'Recording wave should be shown');

    const audioWave: AudioWaveElement|null =
        searchAnimatedGlow!.shadowRoot.querySelector('audio-wave');
    assertFalse(!!audioWave, 'Audio wave should not be shown');
  });

  test('recording wave is hidden when not listening for composebox', async () => {
    loadTimeData.overrideValues({
          voiceSearchCoherenceComposeboxesEnabled: true,
    });
    await createComposeboxElement();

    const mockComposeboxElement =
        composeboxElement as unknown as MockComposebox;
    mockComposeboxElement.inVoiceSearchMode = false;
    await microtasksFinished();

    // SearchAnimatedGlow unconditionally exists
    const searchAnimatedGlow =
        composeboxElement.shadowRoot.querySelector('search-animated-glow');
    await searchAnimatedGlow!.updateComplete;

    const recordingWave: RecordingWaveElement|null =
        searchAnimatedGlow!.shadowRoot.querySelector('recording-wave');
    assertFalse(!!recordingWave, 'Recording wave should not be shown');

    const audioWave: AudioWaveElement|null =
        searchAnimatedGlow!.shadowRoot.querySelector('audio-wave');
    assertFalse(!!audioWave, 'Audio wave should not be shown');
  });

  test('recording wave is rendered when listening for searchbox', async () => {
    loadTimeData.overrideValues({
      voiceSearchCoherenceAnySearchboxExperimentEnabled: true,
    });
    await createComposeboxElement();

    composeboxElement.inVoiceSearchMode = true;
    await microtasksFinished();

    // SearchAnimatedGlow unconditionally exists
    const searchAnimatedGlow =
        composeboxElement.shadowRoot.querySelector('search-animated-glow');
    await searchAnimatedGlow!.updateComplete;
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
          voiceSearchCoherenceAnySearchboxExperimentEnabled: true,
        });
        await createComposeboxElement();

        composeboxElement.inVoiceSearchMode = false;
        await microtasksFinished();

        // SearchAnimatedGlow unconditionally exists
        const searchAnimatedGlow =
            composeboxElement.shadowRoot.querySelector('search-animated-glow');
        await searchAnimatedGlow!.updateComplete;

        const recordingWave: RecordingWaveElement|null =
            searchAnimatedGlow!.shadowRoot.querySelector('recording-wave');
        assertFalse(!!recordingWave, 'Recording wave should not be shown');

        const audioWave: AudioWaveElement|null =
            searchAnimatedGlow!.shadowRoot.querySelector('audio-wave');
        assertFalse(!!audioWave, 'Audio wave should not be shown');
      });

  test(
      'voice search container is empty without webkitSpeechRecognition API',
      async () => {
        // Temporarily remove API
        windowProxy.setResultFor('hasWebkitSpeechRecognition', false);
        await microtasksFinished();

        composeboxElement = document.createElement('cr-composebox');
        composeboxElement.showVoiceSearch = true;
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

  test(
      'onResult_ recovers from STARTED state missing audio and speech events',
      async () => {
        const voiceSearchButton = getVoiceSearchButton(composeboxElement);
        voiceSearchButton!.click();
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

  test(
      'onResult_ recovers from AUDIO_RECEIVED state missing speech event',
      async () => {
        const voiceSearchButton = getVoiceSearchButton(composeboxElement);
        voiceSearchButton!.click();
        await microtasksFinished();

        const voiceSearchElement =
            getVoiceSearchElement(composeboxElement) as any;

        // Simulate audiostart event so the state becomes AUDIO_RECEIVED.
        mockSpeechRecognition.onaudiostart!(new Event('audiostart'));
        await microtasksFinished();

        let speechReceivedFired = false;
        voiceSearchElement.addEventListener('speech-received', () => {
          speechReceivedFired = true;
        });

        const result = createResults(1);
        Object.assign(
            result.results[0]![0]!, {confidence: 1, transcript: 'test2'});
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
      'onResult_ ignores late results in unexpected states like ERROR_RECEIVED',
      async () => {
        const voiceSearchButton = getVoiceSearchButton(composeboxElement);
        voiceSearchButton!.click();
        await microtasksFinished();

        const voiceSearchElement =
            getVoiceSearchElement(composeboxElement) as any;

        // Simulate a network error so the state becomes ERROR_RECEIVED.
        mockSpeechRecognition.onerror!
            ({error: 'network'} as SpeechRecognitionErrorEvent);
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

  test(
      'onResult_ force-submits when interim result exceeds length limit',
      async () => {
        const voiceSearchButton = getVoiceSearchButton(composeboxElement);
        voiceSearchButton!.click();
        await microtasksFinished();

        const voiceSearchElement = getVoiceSearchElement(composeboxElement);

        // Listen for the final result event to verify if it was
        // force-submitted.
        let finalResultFired = false;
        let submittedResult = '';
        voiceSearchElement.addEventListener(
            'voice-search-final-result', (e: any) => {
              finalResultFired = true;
              submittedResult = e.detail;
            });

        // Construct a long string exceeding the 120 character limit.
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

  test(
      'NO_MATCH error auto-closes after 24s when hasErrorTimer is true',
      async () => {
        // Setup.
        const voiceSearchButton = getVoiceSearchButton(composeboxElement);
        voiceSearchButton!.click();
        await microtasksFinished();

        const voiceSearchElement =
            getVoiceSearchElement(composeboxElement) as any;
        voiceSearchElement.hasErrorTimer = true;

        let cancelEventFired = false;
        voiceSearchElement.addEventListener('voice-search-cancel', () => {
          cancelEventFired = true;
        });
        windowProxy.resetResolver('setTimeout');
        mockSpeechRecognition.onnomatch!(new Event('nomatch'));
        await microtasksFinished();

        const [callback, timeoutMs] =
            await windowProxy.whenCalled('setTimeout');

        assertEquals(24000, timeoutMs);

        callback();
        await microtasksFinished();

        // Assert: The voice-search-cancel event should be fired to close the
        // UI.
        assertTrue(cancelEventFired);

        assertEquals(null, voiceSearchElement.detailedError_);

        assertEquals(
            1,
            metrics.count(
                'VoiceSearch.Action.NTP_REALBOX',
                VoiceSearchAction.ERROR_CANCELING));
      });
});

suite('ComposeboxVoiceSearchMetrics', () => {
  let voiceSearchElement: ComposeboxVoiceSearchElement;
  let mockVoiceSearch: MockComposeboxVoiceSearch;
  let metrics: MetricsTracker;
  let handler: TestMock<PageHandlerRemote>;
  let searchboxHandler: TestMock<SearchboxPageHandlerRemote>;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    // Intercept metrics recording.
    metrics = fakeMetricsPrivate();
    handler = TestMock.fromClass(PageHandlerRemote);
    handler.setResultMapperFor(
        'getSmartTabSharingActive', () => Promise.resolve({active: false}));
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
    mockVoiceSearch =
        voiceSearchElement as unknown as MockComposeboxVoiceSearch;
    await microtasksFinished();
  });

  test('Records SUCCESS and SUBMITTED metrics on final result', async () => {
    // Trigger: Simulate receiving the final voice result.
    mockVoiceSearch.onFinalResult_('hello world');
    await microtasksFinished();
    // Verify: Action logged QUERY_SUBMITTED.
    assertEquals(
        1,
        metrics.count(
            'VoiceSearch.Action.NTP_REALBOX',
            VoiceSearchAction.QUERY_SUBMITTED));
  });

  test('Records CANCELED metrics on close button click', async () => {
    // Trigger: Simulate user clicking close.
    mockVoiceSearch.onCloseClick_();
    await microtasksFinished();

    // Verify: Action logged CANCELED_BY_USER.
    assertEquals(
        1,
        metrics.count(
            'VoiceSearch.Action.NTP_REALBOX',
            VoiceSearchAction.CANCELED_BY_USER));
  });

  test('Records ERROR metrics on API error event', async () => {
    // Change parameters to test if dynamic concatenation works.
    searchboxHandler.setResultFor(
        'getPageClassification',
        Promise.resolve({metricSource: 'CO_BROWSING_COMPOSEBOX'}));
    document.body.removeChild(voiceSearchElement);
    document.body.appendChild(voiceSearchElement);
    await microtasksFinished();

    // Trigger: Simulate underlying API throwing an error (network).
    const errorEvent = new Event('error') as any;
    errorEvent.error = 'network';
    mockVoiceSearch.voiceRecognition_.onerror!(errorEvent);

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
    mockVoiceSearch.voiceRecognition_.onerror!(errorEvent);

    // Call onEnd_ to simulate recognition ending, which is when the State is
    // recorded.
    mockVoiceSearch.onEnd_();
    await microtasksFinished();

    // Verify: State logged a non-canceling error (ERROR_NON_CANCELING).
    assertEquals(
        1,
        metrics.count(
            'VoiceSearch.Action.NTP_REALBOX',
            VoiceSearchAction.ERROR_NON_CANCELING));
  });

  test('Records ERROR_NON_CANCELING state for all errors', async () => {
    // Trigger: Simulate a generic error like network.
    const errorEvent = new Event('error') as any;
    errorEvent.error = 'network';

    // Note: Metrics are now recorded immediately in onError_, not in onEnd_.
    mockVoiceSearch.voiceRecognition_.onerror!(errorEvent);
    await microtasksFinished();

    // Verify: State logged a non-canceling error (VOICE_SEARCH_ERROR)
    // because all errors now keep the UI open.
    assertEquals(
        1,
        metrics.count(
            'VoiceSearch.Action.NTP_REALBOX',
            VoiceSearchAction.ERROR_NON_CANCELING));
  });

  test('Records NO_MATCH error on nomatch event', async () => {
    // Trigger: Simulate no match (onnomatch).
    mockVoiceSearch.voiceRecognition_.onnomatch!(new Event('nomatch'));

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
    mockVoiceSearch.onTryAgainClick_(mockRetryEvent);
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

    mockVoiceSearch.onLinkClick_(mockLinkEvent);
    await microtasksFinished();

    assertEquals(
        1,
        metrics.count(
            'VoiceSearch.Action.NTP_REALBOX',
            VoiceSearchAction.SUPPORT_LINK_CLICKED));

    mockVoiceSearch.state_ = -1;
    mockVoiceSearch.voiceRecognition_.abort();
    await microtasksFinished();
  });

  test(
      'Records specific errors in onEnd_ based on fallback state', async () => {
        // Trigger: Force set internal state to STARTED and call onEnd_.
        mockVoiceSearch.state_ = 0;  // State.STARTED
        mockVoiceSearch.onEnd_();
        await microtasksFinished();
        // Verify: Because it ended unexpectedly during STARTED, it should log
        // an AUDIO_CAPTURE error.
        assertEquals(
            1,
            metrics.count(
                'VoiceSearch.Errors.NTP_REALBOX',
                VoiceSearchError.AUDIO_CAPTURE));
      });

  test('Records aggregated base metric for Actions', async () => {
    // Trigger an action: Start voice search via icon click.
    mockVoiceSearch.onCloseClick_();

    // Wait for the async metric recording to complete.
    await microtasksFinished();

    // Verify the sliced metric (e.g., specific to NTP_REALBOX).
    assertEquals(
        1,
        metrics.count(
            'VoiceSearch.Action.NTP_REALBOX',
            VoiceSearchAction.CANCELED_BY_USER));

    // Verify the newly added aggregated base metric (total across all
    // surfaces).
    assertEquals(
        1,
        metrics.count(
            'VoiceSearch.Action', VoiceSearchAction.CANCELED_BY_USER));

    // Clean up internal state to prevent leaking into the next test.
    mockVoiceSearch.state_ = -1;
    mockVoiceSearch.voiceRecognition_.abort();
    await microtasksFinished();
  });

  test('Records aggregated base metric for Errors', async () => {
    // Trigger an error: Simulate a network error.
    const errorEvent = new Event('error') as any;
    errorEvent.error = 'network';
    mockVoiceSearch.voiceRecognition_.onerror!(errorEvent);

    // Wait for the async metric recording to complete.
    await microtasksFinished();

    // Verify the sliced metric (e.g., specific to NTP_REALBOX).
    assertEquals(
        1,
        metrics.count(
            'VoiceSearch.Errors.NTP_REALBOX', VoiceSearchError.NETWORK));

    // Verify the newly added aggregated base metric (total across all
    // surfaces).
    assertEquals(
        1, metrics.count('VoiceSearch.Errors', VoiceSearchError.NETWORK));

    // Clean up internal state to prevent leaking into the next test.
    mockVoiceSearch.state_ = -1;
    mockVoiceSearch.voiceRecognition_.abort();
    await microtasksFinished();
  });

  test('Records ACTIVATED_BY_ICON action on start', async () => {
    // Trigger voice search via icon click.
    voiceSearchElement.start();
    await microtasksFinished();

    // Verify the activation action is logged in both sliced and base metrics.
    assertEquals(
        1,
        metrics.count(
            'VoiceSearch.Action.NTP_REALBOX',
            VoiceSearchAction.ACTIVATED_BY_ICON));
    assertEquals(
        1,
        metrics.count(
            'VoiceSearch.Action', VoiceSearchAction.ACTIVATED_BY_ICON));

    // Clean up internal state to prevent leaking into the next test.
    mockVoiceSearch.state_ = -1;
    mockVoiceSearch.voiceRecognition_.abort();
    await microtasksFinished();
  });

  test('Records CANCELED_BY_USER action on close click', async () => {
    // Simulate a user explicitly closing the voice search overlay.
    mockVoiceSearch.onCloseClick_();
    await microtasksFinished();

    // Verify the cancellation action is logged in both sliced and base metrics.
    assertEquals(
        1,
        metrics.count(
            'VoiceSearch.Action.NTP_REALBOX',
            VoiceSearchAction.CANCELED_BY_USER));
    assertEquals(
        1,
        metrics.count(
            'VoiceSearch.Action', VoiceSearchAction.CANCELED_BY_USER));

    // Clean up internal state to prevent leaking into the next test.
    mockVoiceSearch.state_ = -1;
    mockVoiceSearch.voiceRecognition_.abort();
    await microtasksFinished();
  });

  test('Records ABORTED error but skips action metric recording', async () => {
    // Simulate an aborted error from the underlying speech recognition API.
    mockSpeechRecognition.onerror!
        ({error: 'aborted'} as SpeechRecognitionErrorEvent);
    await microtasksFinished();

    // Verify the aborted error is properly logged in the Errors metrics.
    assertEquals(
        1,
        metrics.count(
            'VoiceSearch.Errors.NTP_REALBOX', VoiceSearchError.ABORTED));
    assertEquals(
        1, metrics.count('VoiceSearch.Errors', VoiceSearchError.ABORTED));

    // Verify no action metrics are logged, as aborted errors should exit early.
    assertEquals(
        0,
        metrics.count(
            'VoiceSearch.Action.NTP_REALBOX',
            VoiceSearchAction.ERROR_CANCELING));
    assertEquals(
        0,
        metrics.count(
            'VoiceSearch.Action.NTP_REALBOX',
            VoiceSearchAction.ERROR_NON_CANCELING));

    // Clean up internal state to prevent leaking into the next test.
    mockVoiceSearch.state_ = -1;
    mockVoiceSearch.voiceRecognition_.abort();
    await microtasksFinished();
  });

  test('Records legacy NTP metrics only for NTP_REALBOX', async () => {
    // This composebox_voice_search component is designed to replace the
    // existing voice_search_overlay.ts on the NTP. Dual-logging the legacy
    // NewTabPage.* metrics here to ensure data continuity during the upcoming
    // UI migration and to validate the accuracy of the new unified
    // VoiceSearch.* metrics. These legacy metrics should be removed entirely
    // once the new metrics are fully validated and approved.
    mockVoiceSearch.metricSource_ = 'NTP_REALBOX';

    voiceSearchElement.$.closeButton.click();
    await microtasksFinished();

    // Verify: The legacy NewTabPage.VoiceActions metric records
    // CLOSE_OVERLAY (value 2), instead of the new CANCELED_BY_USER (value 11).
    assertEquals(
        1, metrics.count('NewTabPage.VoiceActions', /* CLOSE_OVERLAY */ 2));
    assertEquals(
        0,
        metrics.count(
            'NewTabPage.VoiceActions', VoiceSearchAction.CANCELED_BY_USER));

    // Trigger: Simulate a network error.
    mockSpeechRecognition.onerror!
        ({error: 'network'} as SpeechRecognitionErrorEvent);
    await microtasksFinished();

    // Verify: The legacy NewTabPage.VoiceErrors metric records NETWORK.
    assertEquals(
        1, metrics.count('NewTabPage.VoiceErrors', VoiceSearchError.NETWORK));

    // Clean up internal state to prevent leaking into the next test.
    mockVoiceSearch.state_ = -1;
    mockVoiceSearch.voiceRecognition_.abort();
    await microtasksFinished();
  });

  test('Does not record legacy NTP metrics for non-NTP surfaces', async () => {
    mockVoiceSearch.metricSource_ = 'CO_BROWSING_COMPOSEBOX';

    voiceSearchElement.$.closeButton.click();
    mockSpeechRecognition.onerror!
        ({error: 'network'} as SpeechRecognitionErrorEvent);
    await microtasksFinished();

    // Verify: The unified histograms are recorded correctly.
    assertEquals(
        1,
        metrics.count(
            'VoiceSearch.Action.CO_BROWSING_COMPOSEBOX',
            VoiceSearchAction.CANCELED_BY_USER));

    // Verify: The legacy NTP histograms are completely ignored and not
    // polluted.
    assertEquals(0, metrics.count('NewTabPage.VoiceSearch.Action', 2));
    assertEquals(
        0, metrics.count('NewTabPage.VoiceErrors', VoiceSearchError.NETWORK));

    // Clean up internal state to prevent leaking into the next test.
    mockVoiceSearch.state_ = -1;
    mockVoiceSearch.voiceRecognition_.abort();
    await microtasksFinished();
  });

});
