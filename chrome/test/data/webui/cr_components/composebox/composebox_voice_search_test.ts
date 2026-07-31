// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/strings.m.js';
import './test_composebox_mixin.js';
import 'chrome://resources/cr_components/composebox/composebox_voice_search.js';

import {PageHandlerRemote} from 'chrome://resources/cr_components/composebox/composebox.mojom-webui.js';
import {ComposeboxProxyImpl, createAutocompleteMatch} from 'chrome://resources/cr_components/composebox/composebox_proxy.js';
import type {ComposeboxVoiceSearchElement} from 'chrome://resources/cr_components/composebox/composebox_voice_search.js';
import {VoiceSearchAction, VoiceSearchError} from 'chrome://resources/cr_components/composebox/composebox_voice_search.js';
import {WindowProxy} from 'chrome://resources/cr_components/composebox/window_proxy.js';
import {GlowAnimationState} from 'chrome://resources/cr_components/search/constants.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeMediaQueryList} from 'chrome://webui-test/fake_media_query_list.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {$$, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

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
      0: {transcript: 'foo', confidence: 1},
      length: 1,
      isFinal: false,
    } as unknown as SpeechRecognitionResult;
  }) as unknown as SpeechRecognitionResultList;

  return {
    type: 'result',
    resultIndex: 0,
    results: results,
  } as unknown as SpeechRecognitionEvent;
}



suite('ComposeboxVoiceSearch', () => {
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
      composeboxSource: 'NTP_REALBOX',
    });

    windowProxy.setResultMapperFor(
        'createSpeechRecognition',
        () => new MockSpeechRecognition() as unknown as SpeechRecognition);

    await createComposeboxElement();
  });

  teardown(async () => {
    const el = composeboxElement?.shadowRoot?.querySelector<HTMLElement>(
        'cr-composebox-voice-search');
    if (el) {
      const mockVoiceSearch = el as any;
      mockVoiceSearch.state_ = -1;
      mockVoiceSearch.voiceRecognition_?.abort();
      mockVoiceSearch.voiceModeEndCleanup_();
    }
    await microtasksFinished();
  });

  async function createComposeboxElement(showVoiceSearch: boolean = true) {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    composeboxElement = document.createElement('test-composebox-mixin');
    composeboxElement.showVoiceSearch = showVoiceSearch;
    document.body.appendChild(composeboxElement);
    await microtasksFinished();
    disableTransitionsRecursively(composeboxElement);
  }

  function getVoiceSearchButton(composeboxElement: TestComposeboxMixinElement):
      HTMLElement|null {
    return composeboxElement?.shadowRoot?.querySelector<HTMLElement>(
               '#voiceSearchButton') ??
        null;
  }

  function getVoiceSearchElement(composeboxElement: TestComposeboxMixinElement):
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

    return voiceSearchElement;
  }

  test('verifies idle timeout is 3000ms', async () => {
    await openVoiceSearchUI();
    windowProxy.resetResolver('setTimeout');

    // Trigger an audio event to force the timer to reset.
    // This will generate a fresh setTimeout call outside of the
    // openVoiceSearchUI() function.
    mockSpeechRecognition.onaudiostart!(new Event('audiostart'));

    const [, timeoutMs] = await windowProxy.whenCalled('setTimeout');

    // Ensure it matches Google3.
    assertEquals(3000, timeoutMs);
  });

  test('ABORTED error bypasses timer management', async () => {
    const voiceSearchElement = await openVoiceSearchUI();
    const mockVoiceSearch =
        voiceSearchElement as unknown as MockComposeboxVoiceSearch;
    windowProxy.reset();

    mockVoiceSearch.voiceRecognition_.onerror!(new SpeechRecognitionErrorEvent(
        'error', {message: '', error: 'aborted'}));
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
    await createComposeboxElement();

    const voiceSearchElement = await openVoiceSearchUI();

    mockSpeechRecognition.onerror!
        ({error: 'network'} as SpeechRecognitionErrorEvent);
    await microtasksFinished();

    const errorContainer = $$(voiceSearchElement, '#error-container');
    const bottomActions =
        voiceSearchElement.shadowRoot.querySelector<HTMLElement>(
            '#bottomActions');

    assertTrue(!!errorContainer, 'Error container should exist');
    assertFalse(errorContainer.hidden, 'Error container should be visible');
    assertTrue(!!bottomActions, 'Bottom actions container should exist');
    assertStyle(bottomActions, 'opacity', '0');
  });

  test(
      'does not hide cancel button in voice search if flag is false',
      async () => {
        loadTimeData.overrideValues({
          voiceSearchCoherenceComposeboxesEnabled: false,
        });
        await createComposeboxElement();

        const voiceSearchElement = await openVoiceSearchUI();
        let closeButton =
            voiceSearchElement.shadowRoot.querySelector('#closeButton');
        assertTrue(!!closeButton, 'close button should be shown');

        mockSpeechRecognition.onerror!
            ({error: 'network'} as SpeechRecognitionErrorEvent);
        await microtasksFinished();

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
        await createComposeboxElement();

        const voiceSearchElement = await openVoiceSearchUI();
        const closeButton =
            voiceSearchElement.shadowRoot.querySelector('#closeButton');
        assertFalse(!!closeButton, 'close button should be hidden');
      });

  test('shows cancel button when error scrim is shown', async () => {
    loadTimeData.overrideValues({
      voiceSearchCoherenceComposeboxesEnabled: true,
    });
    await createComposeboxElement();

    const voiceSearchElement = await openVoiceSearchUI();

    mockSpeechRecognition.onerror!
        ({error: 'network'} as SpeechRecognitionErrorEvent);
    await microtasksFinished();

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
        windowProxy.resetResolver('setTimeout');
        const mockVoiceSearch =
            voiceSearchElement as unknown as MockComposeboxVoiceSearch;
        // Simulate Composebox behavior.
        mockVoiceSearch.hasErrorTimer = false;

        let cancelEventFired = false;
        voiceSearchElement.addEventListener('voice-search-cancel', () => {
          cancelEventFired = true;
        });

        mockVoiceSearch.voiceRecognition_.onnomatch!(new Event('nomatch'));
        await microtasksFinished();

        // Verify immediate closure with no error message and no timer.
        assertTrue(cancelEventFired);
        assertEquals('', mockVoiceSearch.errorMessage_);
        assertEquals(0, windowProxy.getCallCount('setTimeout'));
        assertEquals(
            1,
            metrics.count(
                'VoiceSearch.Action.NTP_REALBOX',
                VoiceSearchAction.ERROR_CANCELING));
      });

  test(
      'Other errors keep UI open permanently when hasErrorTimer is false',
      async () => {
        const voiceSearchElement = await openVoiceSearchUI();
        const mockVoiceSearch =
            voiceSearchElement as unknown as MockComposeboxVoiceSearch;
        // Simulate Composebox behavior.
        mockVoiceSearch.hasErrorTimer = false;

        mockSpeechRecognition.onerror!
            ({error: 'network'} as SpeechRecognitionErrorEvent);
        await microtasksFinished();

        const errorContainer = $$(voiceSearchElement, '#error-container');
        const inputElement = $$(voiceSearchElement, '#input');

        // Verify the error UI remains open permanently with the correct text.
        assertTrue(!!errorContainer);
        assertFalse(errorContainer.hidden);
        assertStyle(inputElement!, 'opacity', '0');
        assertEquals(
            loadTimeData.getString('networkError'),
            mockVoiceSearch.errorMessage_);

        assertStyle(composeboxElement.$.composebox, 'display', 'none');
        assertStyle(voiceSearchElement, 'display', 'block');

        assertEquals(
            1,
            metrics.count(
                'VoiceSearch.Action.NTP_REALBOX',
                VoiceSearchAction.ERROR_NON_CANCELING));
      });

  test(
      'idle timeout triggers NO_SPEECH and auto-closes instantly in Composebox',
      async () => {
        const voiceSearchElement =
            (await openVoiceSearchUI()) as unknown as MockComposeboxVoiceSearch;
        // Simulate Composebox behavior where timer is disabled.
        voiceSearchElement.hasErrorTimer = false;

        let cancelEventFired = false;
        voiceSearchElement.addEventListener('voice-search-cancel', () => {
          cancelEventFired = true;
        });

        // Intercept the 1.5s idle timer triggered during start().
        const setTimeoutCalls = windowProxy.getArgs('setTimeout');
        assertTrue(setTimeoutCalls.length >= 1);
        const callback = setTimeoutCalls[0][0];
        // Reset resolver to verify no extra timers are created afterwards.
        windowProxy.resetResolver('setTimeout');
        callback();

        await microtasksFinished();

        // Assert: Component state is cleared due to instant resetState_().
        assertEquals(null, voiceSearchElement.detailedError_);
        // Assert: NO_SPEECH error was successfully recorded in metrics.
        assertEquals(
            1,
            metrics.count(
                'VoiceSearch.Errors.NTP_REALBOX', VoiceSearchError.NO_SPEECH));

        // Assert: UI closes instantly with no error message and no new timers.
        assertTrue(cancelEventFired);
        assertEquals('', voiceSearchElement.errorMessage_);
        assertEquals(0, windowProxy.getCallCount('setTimeout'));

        // Assert: The action is logged as ERROR_CANCELING.
        assertEquals(
            1,
            metrics.count(
                'VoiceSearch.Action.NTP_REALBOX',
                VoiceSearchAction.ERROR_CANCELING));
      });

  test(
      'NO_MATCH error auto-closes after 24s when hasErrorTimer is true',
      async () => {
        const voiceSearchElement = await openVoiceSearchUI();
        windowProxy.resetResolver('setTimeout');
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
      });

  test(
      'Other errors auto-close after 9s when hasErrorTimer is true',
      async () => {
        const voiceSearchElement = await openVoiceSearchUI();
        windowProxy.resetResolver('setTimeout');
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
      });

  test(
      'NO_MATCH error renders Try Again link and hides Details link',
      async () => {
        const voiceSearchElement = await openVoiceSearchUI();
        voiceSearchElement.hasErrorTimer = true;

        // Verify initially both links are not visible.
        assertFalse(isVisible(voiceSearchElement.shadowRoot.querySelector('#tryAgainLink')));
        assertFalse(isVisible(voiceSearchElement.shadowRoot.querySelector('#details')));

        // Trigger NO_MATCH error.
        mockSpeechRecognition.onnomatch!(new Event('nomatch'));
        await microtasksFinished();

        // Verify `tryAgainLink` is visible and `details` link is hidden.
        const tryAgainLink =
            voiceSearchElement.shadowRoot.querySelector<HTMLElement>(
                '#tryAgainLink');
        const detailsLink =
            voiceSearchElement.shadowRoot.querySelector<HTMLElement>(
                '#details');

        assertTrue(isVisible(tryAgainLink));
        assertFalse(isVisible(detailsLink));

        assertTrue(!!tryAgainLink);
        tryAgainLink.click();
        await microtasksFinished();
        assertEquals(2, mockSpeechRecognition.startCount);
        assertEquals(null, voiceSearchElement.getErrorForTesting());
        assertEquals('', voiceSearchElement.getErrorMessageForTesting());

        // Trigger other error (like `audio-capture`).
        mockSpeechRecognition.onerror!
            ({error: 'audio-capture'} as any);
        await microtasksFinished();

        // Verify `tryAgainLink` is hidden and `details` link is visible.
        const tryAgainLink2 =
            voiceSearchElement.shadowRoot.querySelector<HTMLElement>(
                '#tryAgainLink');
        const detailsLink2 =
            voiceSearchElement.shadowRoot.querySelector<HTMLElement>(
                '#details');
        assertFalse(isVisible(tryAgainLink2));
        assertTrue(isVisible(detailsLink2));
      });

  test('voice search button does not show when disabled', async () => {
    await createComposeboxElement(false);

    const voiceSearchButton = getVoiceSearchButton(composeboxElement);
    assertFalse(!!voiceSearchButton);
  });

  test('voice search button shows when enabled', () => {
    const voiceSearchButton = getVoiceSearchButton(composeboxElement);
    assertTrue(!!voiceSearchButton);
  });

  test(
      'stop and submit buttons hide when coherence flag is disabled',
      async () => {
        // Disable flag and recreate element to apply new loadTimeData values.
        loadTimeData.overrideValues({
          voiceSearchCoherenceComposeboxesEnabled: false,
        });
        await createComposeboxElement();

        const voiceSearchElement = await openVoiceSearchUI();

        const stopButton =
            voiceSearchElement.shadowRoot.querySelector('#stopButton');
        const submitButton =
            voiceSearchElement.shadowRoot.querySelector('#submitButton');

        assertFalse(
            !!stopButton, 'Stop button should be hidden when flag is disabled');
        assertFalse(
            !!submitButton,
            'Submit button should be hidden when flag is disabled');
      });

  test(
      'Submits the voice transcript directly on submit click while recording',
      async () => {
        const voiceTranscript = 'voice query';

        // Enable flag and set up the component.
        loadTimeData.overrideValues({
          voiceSearchCoherenceComposeboxesEnabled: true,
        });
        await createComposeboxElement();

        const voiceSearchElement = await openVoiceSearchUI();


        // Simulate speech recognition result (user is speaking).
        const speechRes = createResults(1);
        Object.assign(
            speechRes.results[0]![0]!,
            {confidence: 1, transcript: voiceTranscript});
        mockSpeechRecognition.onresult!(speechRes);
        await microtasksFinished();

        const submitButton =
            voiceSearchElement.shadowRoot.querySelector<HTMLElement>(
                '#submitButton');
        assertTrue(!!submitButton, 'Inner submit button should exist');

        // Simulate clicking the inner submit button.
        searchboxHandler.resetResolver('submitQuery');
        submitButton.dispatchEvent(new CustomEvent('submit-click'));
        await microtasksFinished();

        // Verify the voice search engine successfully stopped automatically.
        assertFalse(
            mockSpeechRecognition.voiceSearchInProgress,
            'Voice search should stop automatically after submitting');

        // 7. Verify the submitted query exactly matches the voice transcript.
        assertEquals(1, searchboxHandler.getCallCount('submitQuery'));
        const submitArgs = await searchboxHandler.whenCalled('submitQuery');
        assertEquals(voiceTranscript, submitArgs[0]);

        assertEquals(
            1,
            metrics.count(
                'VoiceSearch.Action.NTP_REALBOX',
                VoiceSearchAction.QUERY_SUBMITTED),
            'QUERY_SUBMITTED metric should be recorded');
      });

  test(
      'Records STOP_BUTTON_CLICKED action and fires event on stop click',
      async () => {
        loadTimeData.overrideValues({
          voiceSearchCoherenceComposeboxesEnabled: true,
        });

        await createComposeboxElement();

        const voiceSearchElement = await openVoiceSearchUI();

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
      });

  test('Stops voice search on outside pointerdown event', async () => {
    loadTimeData.overrideValues({
      voiceSearchCoherenceComposeboxesEnabled: true,
    });
    await createComposeboxElement();

    const voiceSearchElement = await openVoiceSearchUI();
    assertTrue(mockSpeechRecognition.voiceSearchInProgress);

    await windowProxy.whenCalled('setTimeout');

    // start() calls setTimeout TWICE (1st: idle timer, 2nd: outside listeners).
    // Must grab all calls and execute the callback from the SECOND call to
    // attach listeners.
    const setTimeoutCalls = windowProxy.getArgs('setTimeout');
    assertTrue(
        setTimeoutCalls.length >= 2,
        'setTimeout should be called at least twice');
    const listenersCallback = setTimeoutCalls[1][0];
    listenersCallback();  // Attach the pointerdown and blur listeners
    await microtasksFinished();

    let stoppedEventFired = false;
    voiceSearchElement.addEventListener('recording-stopped', () => {
      stoppedEventFired = true;
    });

    // Simulate clicking OUTSIDE the component (on the document body).
    document.body.dispatchEvent(new PointerEvent('pointerdown', {
      bubbles: true,
      composed: true,
    }));
    await microtasksFinished();

    // Verify the recording stopped properly.
    assertTrue(stoppedEventFired, 'Event should fire on outside pointerdown');
    assertFalse(
        mockSpeechRecognition.voiceSearchInProgress, 'Engine should stop');
  });

  test('Does not stop voice search on pointerdown inside composebox', async () => {
    loadTimeData.overrideValues({
      voiceSearchCoherenceComposeboxesEnabled: true,
    });
    await createComposeboxElement();

    const voiceSearchElement = await openVoiceSearchUI();
    assertTrue(mockSpeechRecognition.voiceSearchInProgress);

    await windowProxy.whenCalled('setTimeout');

    const setTimeoutCalls = windowProxy.getArgs('setTimeout');
    assertTrue(
        setTimeoutCalls.length >= 2,
        'setTimeout should be called at least twice');
    const listenersCallback = setTimeoutCalls[1][0];
    listenersCallback();  // Attach the pointerdown and blur listeners
    await microtasksFinished();

    let stoppedEventFired = false;
    voiceSearchElement.addEventListener('recording-stopped', () => {
      stoppedEventFired = true;
    });

    // Simulate clicking INSIDE the composebox (on the composebox element).
    composeboxElement.dispatchEvent(new PointerEvent('pointerdown', {
      bubbles: true,
      composed: true,
    }));
    await microtasksFinished();

    // Verify the recording did NOT stop.
    assertFalse(stoppedEventFired, 'Event should not fire on inside pointerdown');
    assertTrue(
        mockSpeechRecognition.voiceSearchInProgress, 'Engine should not stop');
  });

  test('Stops voice search on window blur event', async () => {
    loadTimeData.overrideValues({
      voiceSearchCoherenceComposeboxesEnabled: true,
    });
    await createComposeboxElement();

    const voiceSearchElement = await openVoiceSearchUI();
    assertTrue(mockSpeechRecognition.voiceSearchInProgress);

    // Grab and execute the SECOND setTimeout callback to attach listeners.
    await windowProxy.whenCalled('setTimeout');
    const setTimeoutCalls = windowProxy.getArgs('setTimeout');
    const listenersCallback = setTimeoutCalls[1][0];
    listenersCallback();
    await microtasksFinished();

    let stoppedEventFired = false;
    voiceSearchElement.addEventListener('recording-stopped', () => {
      stoppedEventFired = true;
    });

    // Simulate the window losing focus (e.g. user clicks inside the iframe).
    windowProxy.resetResolver('setTimeout');
    window.dispatchEvent(new Event('blur'));
    const [blurCallback] = await windowProxy.whenCalled('setTimeout');
    blurCallback();
    await microtasksFinished();

    // Verify the recording stopped properly.
    assertTrue(stoppedEventFired, 'Event should fire on window blur');
    assertFalse(
        mockSpeechRecognition.voiceSearchInProgress, 'Engine should stop');
  });

  test(
      'Permission prompt with zero size keeps voice search open' +
          ' and does not resize parent',
      async () => {
        loadTimeData.overrideValues({
          voiceSearchCoherenceComposeboxesEnabled: true,
        });
        await createComposeboxElement();

        // Configure setTimeout to return unique, incrementing non-zero IDs
        // to verify that the correct timers are cleared.
        let timerCounter = 1;
        windowProxy.setResultMapperFor('setTimeout', () => {
          return timerCounter++;
        });
        windowProxy.getArgs('setTimeout').length = 0;
        windowProxy.getArgs('clearTimeout').length = 0;

        const voiceSearchElement =
            (await openVoiceSearchUI()) as unknown as MockComposeboxVoiceSearch;

        assertTrue(mockSpeechRecognition.voiceSearchInProgress);

        // Track custom resize events fired on composeboxElement.
        let resizeEventFired = false;
        composeboxElement.addEventListener(
            'voice-permission-prompt-changed', () => {
              resizeEventFired = true;
            });

        // Track voice-permission-changed events fired on voiceSearchElement.
        let voicePermissionEventFired = false;
        let voicePermissionEventDetail: any = null;
        voiceSearchElement.addEventListener(
            'voice-permission-changed', (e: Event) => {
              voicePermissionEventFired = true;
              voicePermissionEventDetail = (e as CustomEvent).detail;
            });

        // Grab and execute the setTimeout callback to attach listeners.
        // It is the second setTimeout call after clicking (index 1, ID 2).
        const setTimeoutCalls = windowProxy.getArgs('setTimeout');
        const listenersCallback = setTimeoutCalls[1][0];
        listenersCallback();
        await microtasksFinished();

        // The speech idle timer ID is 1 (index 0, ID 1).
        const idleTimerId = 1;

        // Simulate the window losing focus (blur) BEFORE the permission prompt
        // shows.
        window.dispatchEvent(new Event('blur'));
        await microtasksFinished();

        // Verify: it scheduled the stop callback (delay 100).
        const setTimeoutCallsAfterBlur = windowProxy.getArgs('setTimeout');
        const blurTimerCallIndex =
            setTimeoutCallsAfterBlur.findIndex((args: any) => args[1] === 100);
        assertTrue(blurTimerCallIndex !== -1, 'Should schedule blur timeout');
        const blurTimerId = blurTimerCallIndex + 1;

        const clearTimeoutCountBefore =
            windowProxy.getCallCount('clearTimeout');

        // Simulate Mojo notification: prompt is showing with size 0,0
        const pageCallbackRouter =
            (composeboxElement as unknown as MockComposebox)
                .searchboxCallbackRouter;
        assertTrue(!!pageCallbackRouter);
        const pageRemote = pageCallbackRouter.$.bindNewPipeAndPassRemote();
        pageRemote.onPermissionPromptChanged(true, {width: 0, height: 0});
        await pageRemote.$.flushForTesting();
        await microtasksFinished();

        // Verify: voiceSearchElement property updated.
        assertTrue(voiceSearchElement.isPermissionPromptOpen);

        // Verify: clearTimeout was called for both blurTimerId and idleTimerId.
        const clearTimeoutCalls = windowProxy.getArgs('clearTimeout');
        const clearedIds = clearTimeoutCalls.slice(clearTimeoutCountBefore);
        assertTrue(
            clearedIds.includes(blurTimerId), 'Should clear blur timeout');
        assertTrue(
            clearedIds.includes(idleTimerId), 'Should clear speech idle timer');

        // Verify: voice-permission-changed event was fired.
        assertTrue(voicePermissionEventFired);
        assertTrue(voicePermissionEventDetail.isOpened);
        assertEquals(0, voicePermissionEventDetail.width);
        assertEquals(0, voicePermissionEventDetail.height);

        // Verify: resize was NOT fired to the parent.
        assertFalse(
            resizeEventFired,
            'Resize event should not fire for zero size prompt');


        // Verify: classes were added to elements.
        assertTrue(
            voiceSearchElement.classList.contains('permission-prompt-showing'));

        // Verify: voice search remains open because permission prompt is open.
        assertTrue(
            mockSpeechRecognition.voiceSearchInProgress,
            'Voice search should remain active');

        // Simulate Mojo notification: prompt is closed
        pageRemote.onPermissionPromptChanged(false, {width: 0, height: 0});
        await pageRemote.$.flushForTesting();
        await microtasksFinished();

        assertFalse(voiceSearchElement.isPermissionPromptOpen);
        assertFalse(
            voiceSearchElement.classList.contains('permission-prompt-showing'));

        // Simulate blur event again.
        window.dispatchEvent(new Event('blur'));
        await microtasksFinished();

        // Retrieve scheduled blur timeout and trigger it.
        const setTimeoutCallsFinal = windowProxy.getArgs('setTimeout');
        const finalBlurTimeoutCall =
            setTimeoutCallsFinal.reverse().find((args: any) => args[1] === 100);
        assertTrue(!!finalBlurTimeoutCall, 'Should schedule blur timeout');
        const blurCallback = finalBlurTimeoutCall[0];
        blurCallback();
        await microtasksFinished();

        assertFalse(
            mockSpeechRecognition.voiceSearchInProgress,
            'Voice search should now stop');
      });

  test('Emits clean transcript without duplicates on stop click', async () => {
    // Enable flag and recreate component.
    loadTimeData.overrideValues({
      voiceSearchCoherenceComposeboxesEnabled: true,
    });
    await createComposeboxElement();

    const voiceSearchElement = await openVoiceSearchUI();
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
  });

  test('Submits the voice transcript accurately after stop click', async () => {
    const voiceTranscript = 'voice query';
    searchboxHandler.setResultMapperFor(
        'queryAutocomplete', () => {
          return Promise.resolve({
            result: {
              input: voiceTranscript,
              matches: [
                createAutocompleteMatch({
                  contents: voiceTranscript,
                  fillIntoEdit: voiceTranscript,
                  allowedToBeDefaultMatch: true,
                  destinationUrl: 'about:blank',
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
    await createComposeboxElement();

    const voiceSearchElement = await openVoiceSearchUI();

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
        loadTimeData.overrideValues({
          voiceSearchCoherenceComposeboxesEnabled: true,
        });
        await createComposeboxElement();

        // Reset so the zps query fired on mount does not count.
        searchboxHandler.resetResolver('queryAutocomplete');

        const voiceSearchElement = await openVoiceSearchUI();

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

        // Verify queryAutocomplete was explicitly called to
        // update suggestions.
        assertEquals(1, searchboxHandler.getCallCount('queryAutocomplete'));

        const queryArgs =
            await searchboxHandler.whenCalled('queryAutocomplete');
        assertEquals(composeboxElement.activeQueryId, queryArgs[0]);
        assertEquals('refresh suggestions', queryArgs[1]);
        assertFalse(queryArgs[2]);  // verify preventInlineAutocomplete is false
      });

  test(
      'clicking voice search starts speech recognition and hides the composebox',
      async () => {
        const voiceSearchElement = await openVoiceSearchUI();
        // Clicking the voice search button should start speech recognition.
        assertTrue(mockSpeechRecognition.voiceSearchInProgress);
        assertStyle(composeboxElement.$.composebox, 'display', 'none');
        assertStyle(voiceSearchElement, 'display', 'block');
        assertEquals(
            composeboxElement.animationState, GlowAnimationState.LISTENING);
      });
  test(
      'permission prompt toggles input display and placeholder text',
      async () => {
        loadTimeData.overrideValues({
          voiceWaiting: 'Waiting for permission...',
        });

        // Open voice search to render it
        const voiceSearchElement = await openVoiceSearchUI();

        voiceSearchElement.pageCallbackRouter =
            ComposeboxProxyImpl.getInstance().searchboxCallbackRouter;
        await microtasksFinished();

        // Initially permission prompt is not open.
        assertFalse(voiceSearchElement.isPermissionPromptOpen);

        let permissionEventDetail: any = null;
        voiceSearchElement.addEventListener(
            'voice-permission-changed', (e: Event) => {
              permissionEventDetail = (e as CustomEvent).detail;
            });

        // Fire `onPermissionPromptChanged(true, promptSize)`.
        const searchboxCallbackRouterRemote =
            ComposeboxProxyImpl.getInstance()
                .searchboxCallbackRouter.$.bindNewPipeAndPassRemote();
        searchboxCallbackRouterRemote.onPermissionPromptChanged(
            true, {width: 100, height: 200});
        await searchboxCallbackRouterRemote.$.flushForTesting();
        await microtasksFinished();

        // Verify state, event, and textarea placeholder.
        assertTrue(voiceSearchElement.isPermissionPromptOpen);
        assertTrue(!!permissionEventDetail);
        assertTrue(permissionEventDetail.isOpened);
        assertEquals(100, permissionEventDetail.width);
        assertEquals(200, permissionEventDetail.height);

        const inputDiv =
            voiceSearchElement.shadowRoot.querySelector<HTMLElement>('#input');
        assertTrue(!!inputDiv, 'Input #input should be rendered');
        assertEquals('Waiting for permission...', inputDiv.textContent?.trim());

        // Fire `onPermissionPromptChanged(false, promptSize)`.
        searchboxCallbackRouterRemote.onPermissionPromptChanged(
            false, {width: 0, height: 0});
        await searchboxCallbackRouterRemote.$.flushForTesting();
        await microtasksFinished();

        // Verify state is reset.
        assertFalse(voiceSearchElement.isPermissionPromptOpen);
      });

  test(
      'pointerdown inside the voice search component does not stop recording',
      async () => {
        const voiceSearchElement = await openVoiceSearchUI();

        // Grab and execute the listener registration timeout callback.
        const setTimeoutCalls = windowProxy.getArgs('setTimeout');
        const listenersCallback =
            setTimeoutCalls.find((call: any) => call[1] === 0);
        assertTrue(
            !!listenersCallback, 'Listeners callback should be scheduled');
        listenersCallback[0]();
        await microtasksFinished();

        // Simulate clicking inside the component.
        const event = new PointerEvent('pointerdown', {
          bubbles: true,
          composed: true,
        });
        // Mock composedPath to include the component itself.
        Object.defineProperty(event, 'composedPath', {
          value: () => [voiceSearchElement],
        });

        document.dispatchEvent(event);
        await microtasksFinished();

        // Verify the recording did NOT stop.
        assertTrue(mockSpeechRecognition.voiceSearchInProgress);
      });

  test('blur event is ignored if permission prompt is open', async () => {
    const voiceSearchElement = await openVoiceSearchUI();

    // Grab and execute the listener registration timeout callback.
    const setTimeoutCalls = windowProxy.getArgs('setTimeout');
    const listenersCallback =
        setTimeoutCalls.find((call: any) => call[1] === 0);
    assertTrue(!!listenersCallback, 'Listeners callback should be scheduled');
    listenersCallback[0]();
    await microtasksFinished();
    // Mock permission prompt to be open.
    voiceSearchElement.isPermissionPromptOpen = true;

    windowProxy.reset();

    // Dispatch blur event.
    window.dispatchEvent(new Event('blur'));
    await microtasksFinished();

    // Verify no timeout was scheduled for closing voice search.
    const setTimeoutCallsAfterBlur = windowProxy.getArgs('setTimeout');
    const blurTimeoutCall =
        setTimeoutCallsAfterBlur.find((call: any) => call[1] === 100);
    assertFalse(!!blurTimeoutCall, 'No blur timeout should be scheduled');
  });

  test(
      'blur event schedules timeout to stop' +
          ' recording, cancelled if prompt opens',
      async () => {
        const voiceSearchElement = await openVoiceSearchUI();

        // Grab and execute the listener registration timeout callback.
        const setTimeoutCalls = windowProxy.getArgs('setTimeout');
        const listenersCallback =
            setTimeoutCalls.find((call: any) => call[1] === 0);
        assertTrue(
            !!listenersCallback, 'Listeners callback should be scheduled');
        listenersCallback[0]();
        await microtasksFinished();

        // Ensure permission prompt is closed initially.
        voiceSearchElement.isPermissionPromptOpen = false;

        windowProxy.reset();

        // Dispatch blur event.
        window.dispatchEvent(new Event('blur'));
        await microtasksFinished();

        // Verify a 100ms timeout was scheduled.
        const setTimeoutCallsAfterBlur = windowProxy.getArgs('setTimeout');
        const blurTimeoutCall =
            setTimeoutCallsAfterBlur.find((call: any) => call[1] === 100);
        assertTrue(!!blurTimeoutCall, 'Blur timeout should be scheduled');

        // Case: Permission prompt is opened before timeout fires.
        voiceSearchElement.isPermissionPromptOpen = true;
        const callback = blurTimeoutCall[0];
        callback();
        await microtasksFinished();

        // Recording should still be in progress because permission prompt
        // opened.
        assertTrue(mockSpeechRecognition.voiceSearchInProgress);

        // Case: If permission prompt is closed, timeout stops recording.
        voiceSearchElement.isPermissionPromptOpen = false;
        callback();
        await microtasksFinished();

        // Recording should stop.
        assertFalse(mockSpeechRecognition.voiceSearchInProgress);
      });

  test(
      'container position is absolute when permission prompt is closed ' +
          'and no error; static otherwise',
      async () => {
        // Open voice search via UI.
        const voiceSearchElement = await openVoiceSearchUI();
        voiceSearchElement.liveTranscriptEnabled = false;
        await voiceSearchElement.updateComplete;
        const container =
            voiceSearchElement.shadowRoot.querySelector('#container');
        assertTrue(!!container);

        // Default state: no permission prompt, no error:
        assertFalse(
            voiceSearchElement.hasAttribute('is-permission-prompt-open'));
        assertFalse(container.classList.contains('has-error'));
        assertEquals('absolute', window.getComputedStyle(container).position);
        assertEquals('0px', window.getComputedStyle(container).top);
        assertEquals(
            'absolute', window.getComputedStyle(voiceSearchElement).position);

        // With permission prompt:
        voiceSearchElement.isPermissionPromptOpen = true;
        await voiceSearchElement.updateComplete;

        assertTrue(
            voiceSearchElement.hasAttribute('is-permission-prompt-open'));
        // Should be static since permission prompt is open.
        assertEquals('static', window.getComputedStyle(container).position);
        // Should be absolute since `isListening`='true'.
        assertEquals(
            'absolute', window.getComputedStyle(voiceSearchElement).position);

        // With error but no permission prompt:
        voiceSearchElement.isPermissionPromptOpen = false;
        mockSpeechRecognition.onerror!
            ({error: 'network'} as SpeechRecognitionErrorEvent);
        await microtasksFinished();
        await voiceSearchElement.updateComplete;

        assertTrue(container.classList.contains('has-error'));
        // Should be static since error.
        assertEquals('static', window.getComputedStyle(container).position);
        // Should be static since `isListening`='false' due to error.
        assertEquals(
            'static', window.getComputedStyle(voiceSearchElement).position);

        // Clean up.
        voiceSearchElement.voiceModeEndCleanupForTesting();
        await microtasksFinished();
      });

  test('transcript input font size uses 16px default', async () => {
    await createComposeboxElement();
    const voiceSearchElement = await openVoiceSearchUI();
    voiceSearchElement.liveTranscriptEnabled = true;
    await voiceSearchElement.updateComplete;

    const input =
        voiceSearchElement.shadowRoot.querySelector<HTMLElement>('#input')!;
    assertEquals('16px', window.getComputedStyle(input).fontSize);
  });

  test(
      'input ends above bottom action buttons and spans full width',
      async () => {
        await createComposeboxElement();
        const voiceSearchElement = await openVoiceSearchUI();
        voiceSearchElement.submitStopButtonsEnabled = true;
        voiceSearchElement.liveTranscriptEnabled = true;
        await voiceSearchElement.updateComplete;

        const container =
            voiceSearchElement.shadowRoot.querySelector<HTMLElement>(
                '#container')!;
        const input =
            voiceSearchElement.shadowRoot.querySelector<HTMLElement>('#input')!;
        const buttonSpacer =
            voiceSearchElement.shadowRoot.querySelector<HTMLElement>(
                '#button-spacer');
        const bottomActions =
            voiceSearchElement.shadowRoot.querySelector<HTMLElement>(
                '#bottomActions')!;

        assertFalse(!!buttonSpacer);
        assertEquals(
            'column', window.getComputedStyle(container).flexDirection);
        assertEquals('20px', window.getComputedStyle(input).paddingInlineEnd);
        assertEquals('0px', window.getComputedStyle(input).paddingBottom);
        assertEquals('static', window.getComputedStyle(bottomActions).position);
      });

  test('voice search text elements inherit composebox font size', async () => {
    await createComposeboxElement();
    const voiceSearchElement = getVoiceSearchElement(composeboxElement);

    composeboxElement.style.setProperty('--cr-composebox-font-size', '14px');

    mockSpeechRecognition.onerror!
        ({error: 'network'} as SpeechRecognitionErrorEvent);
    voiceSearchElement.liveTranscriptEnabled = true;
    await microtasksFinished();
    await voiceSearchElement.updateComplete;

    const input =
        voiceSearchElement.shadowRoot.querySelector<HTMLElement>('#input')!;
    const errorContainer =
        voiceSearchElement.shadowRoot.querySelector<HTMLElement>(
            '#error-container')!;

    const aimInput =
        composeboxElement.shadowRoot.querySelector('cr-composebox-input')!
            .shadowRoot.querySelector<HTMLElement>('#input')!;

    let aimFontSize = window.getComputedStyle(aimInput).fontSize;

    assertEquals(aimFontSize, window.getComputedStyle(input).fontSize);
    assertEquals(aimFontSize, window.getComputedStyle(errorContainer).fontSize);

    // Verify dynamic updates.
    composeboxElement.style.setProperty('--cr-composebox-font-size', '20px');
    await microtasksFinished();
    await voiceSearchElement.updateComplete;

    aimFontSize = window.getComputedStyle(aimInput).fontSize;

    assertEquals(aimFontSize, window.getComputedStyle(input).fontSize);
    assertEquals(aimFontSize, window.getComputedStyle(errorContainer).fontSize);
  });
});
