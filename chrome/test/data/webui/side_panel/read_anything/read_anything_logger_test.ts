// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {MetricsBrowserProxyImpl, ReadAloudSettingsChange, ReadAnythingLogger, ReadAnythingSettingsChange} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals} from 'chrome-untrusted://webui-test/chai_assert.js';
// <if expr="chromeos_ash">
import {ReadAnythingVoiceType} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {createSpeechSynthesisVoice} from './common.js';
// </if>
import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';

suite('Logger', () => {
  let logger: ReadAnythingLogger;
  let metrics: TestMetricsBrowserProxy;

  setup(() => {
    metrics = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metrics);

    logger = new ReadAnythingLogger();
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

  test('highlight on', () => {
    logger.logHighlightState(true);
    logger.logHighlightState(true);

    assertEquals(2, metrics.getCallCount('recordHighlightOn'));
    assertEquals(0, metrics.getCallCount('recordHighlightOff'));
  });

  test('highlight off', () => {
    logger.logHighlightState(false);
    logger.logHighlightState(false);

    assertEquals(0, metrics.getCallCount('recordHighlightOn'));
    assertEquals(2, metrics.getCallCount('recordHighlightOff'));
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

  test('logVoiceSpeed', () => {
    logger.logVoiceSpeed(1);
    logger.logVoiceSpeed(2);
    logger.logVoiceSpeed(5);
    logger.logVoiceSpeed(1);
    logger.logVoiceSpeed(0);

    assertEquals(5, metrics.getCallCount('recordVoiceSpeed'));
  });

  // <if expr="chromeos_ash">
  test('logVoiceTypeUsedForReading with natural voice', async () => {
    logger.logVoiceTypeUsedForReading(
        createSpeechSynthesisVoice({name: 'Grumpy (Natural)'}));

    assertEquals(
        ReadAnythingVoiceType.NATURAL,
        await metrics.whenCalled('recordVoiceType'));
  });

  test('logVoiceTypeUsedForReading with espeak voice', async () => {
    logger.logVoiceTypeUsedForReading(
        createSpeechSynthesisVoice({name: 'eSpeak Happy'}));

    assertEquals(
        ReadAnythingVoiceType.ESPEAK,
        await metrics.whenCalled('recordVoiceType'));
  });

  test('logVoiceTypeUsedForReading with other voice', async () => {
    logger.logVoiceTypeUsedForReading(
        createSpeechSynthesisVoice({name: 'Sleepy'}));

    assertEquals(
        ReadAnythingVoiceType.CHROMEOS,
        await metrics.whenCalled('recordVoiceType'));
  });
  // </if>

  test('logLanguageUsedForReading supported locale', async () => {
    const lang = 'pt-BR';
    logger.logLanguageUsedForReading(lang);
    assertEquals(lang, await metrics.whenCalled('recordLanguage'));
  });

  test('logLanguageUsedForReading supported lang', async () => {
    const lang = 'it';
    logger.logLanguageUsedForReading(lang);
    assertEquals(lang, await metrics.whenCalled('recordLanguage'));
  });

  test(
      'logLanguageUsedForReading unsupported locale with supported base lang',
      async () => {
        const lang = 'id-id';
        logger.logLanguageUsedForReading(lang);
        assertEquals('id', await metrics.whenCalled('recordLanguage'));
      });

  test(
      'logLanguageUsedForReading supported locale with same base lang',
      async () => {
        const lang = 'fr-fr';
        logger.logLanguageUsedForReading(lang);
        assertEquals('fr', await metrics.whenCalled('recordLanguage'));
      });
});
