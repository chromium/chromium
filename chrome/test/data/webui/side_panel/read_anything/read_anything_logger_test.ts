// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {MetricsBrowserProxyImpl, ReadAloudSettingsChange, ReadAnythingLogger, ReadAnythingSettingsChange, ReadAnythingVoiceType, SpeechControls, TimeFrom} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertGT, assertLE} from 'chrome-untrusted://webui-test/chai_assert.js';

import {createSpeechSynthesisVoice} from './common.js';
import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';

suite('Logger', () => {
  const defaultSpeechStartTime = 0;

  let logger: ReadAnythingLogger;
  let metrics: TestMetricsBrowserProxy;

  async function assertTimeMetricIsCalled(
      from: TimeFrom, expectedMetric: string) {
    metrics.reset();
    logger.logTimeFrom(from, 100, 175);
    assertEquals(expectedMetric, (await metrics.whenCalled('recordTime'))[0]);
  }

  setup(() => {
    metrics = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metrics);

    logger = new ReadAnythingLogger();
  });

  test('speech controls', async () => {
    logger.logSpeechControlClick(SpeechControls.NEXT);
    assertEquals(
        'Accessibility.ReadAnything.ReadAloudNextButtonSessionCount',
        await metrics.whenCalled('incrementMetricCount'));
    metrics.reset();

    logger.logSpeechControlClick(SpeechControls.PLAY);
    assertEquals(
        'Accessibility.ReadAnything.ReadAloudPlaySessionCount',
        await metrics.whenCalled('incrementMetricCount'));
  });

  test('with speech logs speech played', () => {
    logger.logNewPage(true);
    logger.logNewPage(true);

    assertEquals(0, metrics.getCallCount('recordNewPage'));
    assertEquals(2, metrics.getCallCount('recordNewPageWithSpeech'));
  });

  test('without speech logs simple new page', () => {
    logger.logNewPage(false);
    logger.logNewPage(false);

    assertEquals(2, metrics.getCallCount('recordNewPage'));
    assertEquals(0, metrics.getCallCount('recordNewPageWithSpeech'));
  });

  test('highlight granularity change', () => {
    logger.logHighlightGranularity(0);
    logger.logHighlightGranularity(4);

    assertEquals(2, metrics.getCallCount('recordHighlightGranularity'));
  });

  test('text settings', async () => {
    logger.logTextSettingsChange(ReadAnythingSettingsChange.FONT_SIZE_CHANGE);
    assertEquals(
        ReadAnythingSettingsChange.FONT_SIZE_CHANGE,
        await metrics.whenCalled('recordTextSettingsChange'));
    assertEquals(0, metrics.getCallCount('recordSpeechSettingsChange'));
    metrics.reset();

    logger.logTextSettingsChange(
        ReadAnythingSettingsChange.LINKS_ENABLED_CHANGE);
    assertEquals(
        ReadAnythingSettingsChange.LINKS_ENABLED_CHANGE,
        await metrics.whenCalled('recordTextSettingsChange'));
    assertEquals(0, metrics.getCallCount('recordSpeechSettingsChange'));
  });

  test('speech settings', async () => {
    logger.logSpeechSettingsChange(ReadAloudSettingsChange.VOICE_NAME_CHANGE);
    assertEquals(
        ReadAloudSettingsChange.VOICE_NAME_CHANGE,
        await metrics.whenCalled('recordSpeechSettingsChange'));
    assertEquals(0, metrics.getCallCount('recordTextSettingsChange'));
    metrics.reset();

    logger.logSpeechSettingsChange(ReadAloudSettingsChange.HIGHLIGHT_CHANGE);
    assertEquals(
        ReadAloudSettingsChange.HIGHLIGHT_CHANGE,
        await metrics.whenCalled('recordSpeechSettingsChange'));
    assertEquals(0, metrics.getCallCount('recordTextSettingsChange'));
  });

  test('speech stop source', async () => {
    const source = 123;
    logger.logSpeechStopSource(source);
    assertEquals(source, await metrics.whenCalled('recordSpeechStopSource'));
  });

  test('empty state', () => {
    logger.logEmptyState();
    assertEquals(1, metrics.getCallCount('recordEmptyState'));
  });

  test('logVoiceSpeed', () => {
    logger.logVoiceSpeed(1);
    logger.logVoiceSpeed(2);
    logger.logVoiceSpeed(5);
    logger.logVoiceSpeed(1);
    logger.logVoiceSpeed(0);

    assertEquals(5, metrics.getCallCount('recordVoiceSpeed'));
  });

  test('logSpeechPlaySession with natural voice', async () => {
    logger.logSpeechPlaySession(
        defaultSpeechStartTime,
        createSpeechSynthesisVoice({name: 'Grumpy (Natural)'}));

    assertEquals(
        ReadAnythingVoiceType.NATURAL,
        await metrics.whenCalled('recordVoiceType'));
  });

  test('logSpeechPlaySession with espeak voice', async () => {
    logger.logSpeechPlaySession(
        defaultSpeechStartTime,
        createSpeechSynthesisVoice({name: 'eSpeak Happy'}));

    assertEquals(
        ReadAnythingVoiceType.ESPEAK,
        await metrics.whenCalled('recordVoiceType'));
  });

  // <if expr="is_chromeos">
  test('logSpeechPlaySession with other voice on ChromeOS', async () => {
    logger.logSpeechPlaySession(
        defaultSpeechStartTime, createSpeechSynthesisVoice({name: 'Sleepy'}));

    assertEquals(
        ReadAnythingVoiceType.CHROMEOS,
        await metrics.whenCalled('recordVoiceType'));
  });
  // </if>

  // <if expr="not is_chromeos">
  test('logSpeechPlaySession with other voice on Desktop', async () => {
    logger.logSpeechPlaySession(
        defaultSpeechStartTime, createSpeechSynthesisVoice({name: 'Dopey'}));

    assertEquals(
        ReadAnythingVoiceType.SYSTEM,
        await metrics.whenCalled('recordVoiceType'));
  });
  // </if>

  test('logSpeechPlaySession supported locale', async () => {
    const lang = 'pt-BR';
    logger.logSpeechPlaySession(
        defaultSpeechStartTime, createSpeechSynthesisVoice({lang}));
    assertEquals(lang, await metrics.whenCalled('recordLanguage'));
  });

  test('logSpeechPlaySession supported lang', async () => {
    const lang = 'it';
    logger.logSpeechPlaySession(
        defaultSpeechStartTime, createSpeechSynthesisVoice({lang}));
    assertEquals(lang, await metrics.whenCalled('recordLanguage'));
  });

  test(
      'logSpeechPlaySession unsupported locale with supported base lang',
      async () => {
        const lang = 'id-id';
        logger.logSpeechPlaySession(
            defaultSpeechStartTime, createSpeechSynthesisVoice({lang}));
        assertEquals('id', await metrics.whenCalled('recordLanguage'));
      });

  test(
      'logSpeechPlaySession supported locale with same base lang', async () => {
        const lang = 'fr-fr';
        logger.logSpeechPlaySession(
            defaultSpeechStartTime, createSpeechSynthesisVoice({lang}));
        assertEquals('fr', await metrics.whenCalled('recordLanguage'));
      });

  test('logSpeechPlaySession records time since start', async () => {
    const startTime = Date.now();
    const expectedTime = 100;

    await new Promise(resolve => setTimeout(resolve, expectedTime));
    logger.logSpeechPlaySession(startTime, null);

    // The playback length should be at least the amount of time we waited above
    // and less than the starting time (i.e. we should be recording length of
    // time, not timestamp).
    const recordedTime = await metrics.whenCalled('recordSpeechPlaybackLength');
    assertLE(expectedTime, recordedTime);
    assertGT(startTime, recordedTime);
  });

  test('logTimeFrom uses correct uma name', () => {
    assertTimeMetricIsCalled(
        TimeFrom.APP,
        'Accessibility.ReadAnything.TimeFromAppStartedToConstructor');
    assertTimeMetricIsCalled(
        TimeFrom.TOOLBAR,
        'Accessibility.ReadAnything.TimeFromToolbarStartedToConstructor');
  });

  test('logTimeFrom logs time difference', async () => {
    const startTime = 50;
    const endTime = 125;
    const expectedTime = endTime - startTime;

    logger.logTimeFrom(TimeFrom.APP, startTime, endTime);

    assertEquals(expectedTime, (await metrics.whenCalled('recordTime'))[1]);
  });
});
