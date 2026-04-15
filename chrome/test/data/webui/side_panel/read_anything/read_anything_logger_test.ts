// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {LinkStatus, MetricsBrowserProxyImpl, ReadAloudSettingsChange, ReadAnythingLogger, ReadAnythingSettingsChange, ReadAnythingVoiceType, SpeechControls, TimeFrom} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertGT, assertLE} from 'chrome-untrusted://webui-test/chai_assert.js';

import {createSpeechSynthesisVoice} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
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
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
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
    metrics.reset();

    logger.logSpeechControlClick(SpeechControls.PLAY_FROM_SELECTION);
    assertEquals(
        'Accessibility.ReadAnything.ReadAloudPlayFromSelectionSessionCount',
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

  test('logVoiceLanguageChange', () => {
    const voice1 = createSpeechSynthesisVoice({lang: 'en-US'});
    const voice2 = createSpeechSynthesisVoice({lang: 'en-UK'});
    const voice3 = createSpeechSynthesisVoice({lang: 'fr-FR'});

    // Same language should not log
    logger.logVoiceLanguageChange(voice1, voice1);
    assertEquals(0, metrics.getCallCount('recordVoiceLanguageChange'));

    // Different locale, same base language should log (e.g. en-US to en-UK)
    logger.logVoiceLanguageChange(voice1, voice2);
    assertEquals(1, metrics.getCallCount('recordVoiceLanguageChange'));
    metrics.reset();

    // Different language should log
    logger.logVoiceLanguageChange(voice1, voice3);
    assertEquals(1, metrics.getCallCount('recordVoiceLanguageChange'));
    metrics.reset();

    // Null voices should not log
    logger.logVoiceLanguageChange(null, voice1);
    logger.logVoiceLanguageChange(voice1, null);
    logger.logVoiceLanguageChange(null, null);
    assertEquals(0, metrics.getCallCount('recordVoiceLanguageChange'));
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

  test('when hidden does not log new page', () => {
    logger.setHidden(true);
    logger.logNewPage(false);
    logger.logNewPage(true);

    assertEquals(0, metrics.getCallCount('recordNewPage'));
    assertEquals(0, metrics.getCallCount('recordNewPageWithSpeech'));

    logger.setHidden(false);
    logger.logNewPage(false);
    logger.logNewPage(true);

    assertEquals(1, metrics.getCallCount('recordNewPage'));
    assertEquals(1, metrics.getCallCount('recordNewPageWithSpeech'));
  });

  test('when hidden does not log empty state', () => {
    logger.setHidden(true);
    logger.logEmptyState();

    assertEquals(0, metrics.getCallCount('recordEmptyState'));

    logger.setHidden(false);
    logger.logEmptyState();

    assertEquals(1, metrics.getCallCount('recordEmptyState'));
  });

  test('line focus session with flag enabled', () => {
    chrome.readingMode.isLineFocusEnabled = true;
    logger.logLineFocusSession();
    assertEquals(1, metrics.getCallCount('recordLineFocusSession'));
  });

  test('line focus session with flag disabled', () => {
    chrome.readingMode.isLineFocusEnabled = false;
    logger.logLineFocusSession();
    assertEquals(0, metrics.getCallCount('recordLineFocusSession'));
  });

  test('line focus toggled with flag enabled', () => {
    chrome.readingMode.isLineFocusEnabled = true;
    logger.logLineFocusToggled(true);
    logger.logLineFocusToggled(false);
    assertEquals(2, metrics.getCallCount('recordLineFocusToggled'));
  });

  test('line focus toggled with flag disabled', () => {
    chrome.readingMode.isLineFocusEnabled = false;
    logger.logLineFocusToggled(true);
    logger.logLineFocusToggled(false);
    assertEquals(0, metrics.getCallCount('recordLineFocusToggled'));
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

  test('logLinkStatusCount', async () => {
    logger.logLinkStatusCount(LinkStatus.NO_HREF, 10);
    logger.logLinkStatusCount(LinkStatus.NO_MATCH, 20);
    logger.logLinkStatusCount(LinkStatus.TOO_MANY_MATCHES, 30);
    logger.logLinkStatusCount(LinkStatus.SUCCESS, 40);

    assertEquals(4, metrics.getCallCount('recordCount'));
    assertEquals(
        'Accessibility.ReadAnything.Readability.PageLinksNoHrefCount',
        (await metrics.whenCalled('recordCount'))[0]);
  });
});
