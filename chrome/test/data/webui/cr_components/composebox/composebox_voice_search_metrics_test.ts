// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/strings.m.js';
import 'chrome://resources/cr_components/composebox/composebox_voice_search.js';

import {PageCallbackRouter, PageHandlerRemote} from 'chrome://resources/cr_components/composebox/composebox.mojom-webui.js';
import {ComposeboxProxyImpl} from 'chrome://resources/cr_components/composebox/composebox_proxy.js';
import type {ComposeboxVoiceSearchElement} from 'chrome://resources/cr_components/composebox/composebox_voice_search.js';
import {VoiceSearchAction, VoiceSearchError, VoiceSearchMetricType} from 'chrome://resources/cr_components/composebox/composebox_voice_search.js';
import {WindowProxy} from 'chrome://resources/cr_components/composebox/window_proxy.js';
import {PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {disableTransitionsRecursively, installMock, MockSpeechRecognition, mockSpeechRecognition} from './composebox_test_utils.js';
import type {MockComposeboxVoiceSearch} from './composebox_test_utils.js';

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

    const windowProxy = installMock(WindowProxy);
    windowProxy.setResultMapperFor(
        'createSpeechRecognition',
        () => new MockSpeechRecognition() as unknown as SpeechRecognition);



    voiceSearchElement = document.createElement('cr-composebox-voice-search');

    document.body.appendChild(voiceSearchElement);
    disableTransitionsRecursively(voiceSearchElement);
    mockVoiceSearch =
        voiceSearchElement as unknown as MockComposeboxVoiceSearch;
    await searchboxHandler.whenCalled('getPageClassification');
    await microtasksFinished();
  });

  test('Records SUCCESS and SUBMITTED metrics on final result', async () => {
    // Trigger: Simulate receiving the final voice result.
    mockVoiceSearch.onFinalResult_('hello world', /*forceSubmit=*/ true);
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
    voiceSearchElement.metricSource = '';
    searchboxHandler.resetResolver('getPageClassification');
    searchboxHandler.setResultFor(
        'getPageClassification',
        Promise.resolve({metricSource: 'CO_BROWSING_COMPOSEBOX'}));
    document.body.removeChild(voiceSearchElement);
    document.body.appendChild(voiceSearchElement);
    await searchboxHandler.whenCalled('getPageClassification');
    await microtasksFinished();

    const errorEvent = new SpeechRecognitionErrorEvent(
        'error', {message: '', error: 'network'});
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
    const errorEvent = new SpeechRecognitionErrorEvent(
        'error', {message: '', error: 'not-allowed'});
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
    const errorEvent = new SpeechRecognitionErrorEvent(
        'error', {message: '', error: 'network'});

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
        mockLinkEvent, 'currentTarget', {value: {href: 'about:blank'}});

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
    const errorEvent = new SpeechRecognitionErrorEvent(
        'error', {message: '', error: 'network'});
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
    mockVoiceSearch.voiceRecognition_.onerror!(new SpeechRecognitionErrorEvent(
        'error', {message: '', error: 'aborted'}));
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
    mockVoiceSearch.metricSource = 'NTP_REALBOX';

    voiceSearchElement.$.closeButton.click();
    await microtasksFinished();

    // Verify: The legacy NewTabPage.VoiceActions metric records
    // CLOSE_OVERLAY, instead of the new CANCELED_BY_USER.
    const LEGACY_ACTION_CLOSE_OVERLAY = 2;
    assertEquals(
        1, metrics.count('NewTabPage.VoiceActions', LEGACY_ACTION_CLOSE_OVERLAY));
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
    searchboxHandler.setResultFor(
        'getPageClassification',
        Promise.resolve({metricSource: 'CO_BROWSING_COMPOSEBOX'}));
    mockVoiceSearch.metricSource = 'CO_BROWSING_COMPOSEBOX';

    mockVoiceSearch.onCloseClick_();
    await microtasksFinished();

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
    const LEGACY_ACTION_CLOSE_OVERLAY = 2;
    assertEquals(0, metrics.count('NewTabPage.VoiceActions', LEGACY_ACTION_CLOSE_OVERLAY));
    assertEquals(
        0, metrics.count('NewTabPage.VoiceErrors', VoiceSearchError.NETWORK));

    // Clean up internal state to prevent leaking into the next test.
    mockVoiceSearch.state_ = -1;
    mockVoiceSearch.voiceRecognition_.abort();
    await microtasksFinished();
  });


  test(
      'Filters new VoiceSearchAction values from legacy NTP metrics during dual-logging',
      async () => {
        const LEGACY_ACTION_CLOSE_OVERLAY = 2;
        mockVoiceSearch.metricSource = 'NTP_REALBOX';

        // Verify legacy action ACTIVATED_BY_ICON is dual-logged to legacy.
        voiceSearchElement.start();
        await microtasksFinished();
        assertEquals(
            1,
            metrics.count(
                'VoiceSearch.Action.NTP_REALBOX',
                VoiceSearchAction.ACTIVATED_BY_ICON));
        assertEquals(
            1,
            metrics.count(
                'NewTabPage.VoiceActions', VoiceSearchAction.ACTIVATED_BY_ICON));

        // Verify CANCELED_BY_USER is mapped to CLOSE_OVERLAY in legacy.
        mockVoiceSearch.onCloseClick_();
        await microtasksFinished();
        assertEquals(
            1,
            metrics.count(
                'VoiceSearch.Action.NTP_REALBOX',
                VoiceSearchAction.CANCELED_BY_USER));
        assertEquals(
            1,
            metrics.count(
                'NewTabPage.VoiceActions', LEGACY_ACTION_CLOSE_OVERLAY));

        // Verify new action STOP_BUTTON_CLICKED is NOT logged in legacy.
        mockVoiceSearch.onStopClick_();
        await microtasksFinished();
        assertEquals(
            1,
            metrics.count(
                'VoiceSearch.Action.NTP_REALBOX',
                VoiceSearchAction.STOP_BUTTON_CLICKED));
        assertEquals(
            0,
            metrics.count(
                'NewTabPage.VoiceActions',
                VoiceSearchAction.STOP_BUTTON_CLICKED));

        // Verify new action RESUME_BUTTON_CLICKED is NOT logged in legacy.
        mockVoiceSearch.recordMetric_(
            VoiceSearchMetricType.ACTION,
            VoiceSearchAction.RESUME_BUTTON_CLICKED,
            VoiceSearchAction.MAX_VALUE + 1);
        await microtasksFinished();
        assertEquals(
            1,
            metrics.count(
                'VoiceSearch.Action.NTP_REALBOX',
                VoiceSearchAction.RESUME_BUTTON_CLICKED));
        assertEquals(
            0,
            metrics.count(
                'NewTabPage.VoiceActions',
                VoiceSearchAction.RESUME_BUTTON_CLICKED));

        // Clean up internal state to prevent leaking into the next test.
        mockVoiceSearch.state_ = -1;
        mockVoiceSearch.voiceRecognition_.abort();
        await microtasksFinished();
      });
});
