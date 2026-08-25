// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {AudioBrowserProxy} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {FakeChromeEvent} from 'chrome-untrusted://webui-test/fake_chrome_event.js';
import {TestBrowserProxy} from 'chrome-untrusted://webui-test/test_browser_proxy.js';

export class TestAudioBrowserProxy extends TestBrowserProxy implements
    AudioBrowserProxy {
  languageChanged = new FakeChromeEvent();
  onLockScreen = new FakeChromeEvent();
  onTabMuteStateChange = new FakeChromeEvent();
  onTtsEngineInstalled = new FakeChromeEvent();
  readingModeWillClose = new FakeChromeEvent();
  setPlayOnOpen = new FakeChromeEvent();
  updateVoicePackStatus = new FakeChromeEvent();

  speechRate: number = 1.0;
  highlightGranularity: number = 0;
  autoHighlighting: number = 0;
  wordHighlighting: number = 1;
  phraseHighlighting: number = 2;
  sentenceHighlighting: number = 3;
  noHighlighting: number = 4;
  isPhraseHighlightingEnabledFlag: boolean = false;
  defaultLanguageForSpeech: string = '';
  baseLanguageForSpeech: string = '';
  storedVoice: string = '';
  languagesEnabledInPref: Set<string> = new Set<string>();
  installedLangs: string[] = [];
  uninstalledLangs: string[] = [];
  requestInfoLangs: string[] = [];
  keyboardShortcutStopSource: number = 0;
  engineErrorStopSource: number = 1;
  engineInterruptStopSource: number = 2;
  contentFinishedStopSource: number = 3;
  pauseButtonStopSource: number = 4;
  unexpectedUpdateContentStopSource: number = 5;
  localeToDisplayName: {[key: string]: string} = {};

  constructor() {
    super([
      'getSpeechRate',
      'getHighlightGranularity',
      'getAutoHighlighting',
      'getWordHighlighting',
      'getPhraseHighlighting',
      'getSentenceHighlighting',
      'getNoHighlighting',
      'isPhraseHighlightingEnabled',
      'getDisplayNameForLocale',
      'getDefaultLanguageForSpeech',
      'getBaseLanguageForSpeech',
      'getStoredVoice',
      'getLanguagesEnabledInPref',
      'isHighlightOn',
      'onSpeechRateChange',
      'onHighlightGranularityChanged',
      'onVoiceChange',
      'onLanguagePrefChange',
      'sendGetVoicePackInfoRequest',
      'sendInstallVoicePackRequest',
      'sendUninstallVoiceRequest',
      'onIsSpeechActiveChanged',
      'onIsAudioCurrentlyPlayingChanged',
      'onSpeechEngineFirstStall',
      'onSpeechEngineStalled',
      'getKeyboardShortcutStopSource',
      'getEngineErrorStopSource',
      'getEngineInterruptStopSource',
      'getContentFinishedStopSource',
      'getPauseButtonStopSource',
      'getUnexpectedUpdateContentStopSource',
    ]);
  }

  getSpeechRate(): number {
    this.methodCalled('getSpeechRate');
    return this.speechRate;
  }

  getHighlightGranularity(): number {
    this.methodCalled('getHighlightGranularity');
    return this.highlightGranularity;
  }

  getAutoHighlighting(): number {
    this.methodCalled('getAutoHighlighting');
    return this.autoHighlighting;
  }

  getWordHighlighting(): number {
    this.methodCalled('getWordHighlighting');
    return this.wordHighlighting;
  }

  getPhraseHighlighting(): number {
    this.methodCalled('getPhraseHighlighting');
    return this.phraseHighlighting;
  }

  getSentenceHighlighting(): number {
    this.methodCalled('getSentenceHighlighting');
    return this.sentenceHighlighting;
  }

  getNoHighlighting(): number {
    this.methodCalled('getNoHighlighting');
    return this.noHighlighting;
  }

  isPhraseHighlightingEnabled(): boolean {
    this.methodCalled('isPhraseHighlightingEnabled');
    return this.isPhraseHighlightingEnabledFlag;
  }

  getDisplayNameForLocale(locale: string, displayLocale: string): string {
    this.methodCalled('getDisplayNameForLocale', locale, displayLocale);
    return this.localeToDisplayName[locale] || locale;
  }

  getDefaultLanguageForSpeech(): string {
    this.methodCalled('getDefaultLanguageForSpeech');
    return this.defaultLanguageForSpeech;
  }

  getBaseLanguageForSpeech(): string {
    this.methodCalled('getBaseLanguageForSpeech');
    return this.baseLanguageForSpeech;
  }

  getStoredVoice(): string {
    this.methodCalled('getStoredVoice');
    return this.storedVoice;
  }

  getLanguagesEnabledInPref(): string[] {
    this.methodCalled('getLanguagesEnabledInPref');
    return Array.from(this.languagesEnabledInPref);
  }

  isHighlightOn(): boolean {
    this.methodCalled('isHighlightOn');
    return this.highlightGranularity !== this.noHighlighting;
  }

  onSpeechRateChange(rate: number): void {
    this.methodCalled('onSpeechRateChange', rate);
    this.speechRate = rate;
  }

  onHighlightGranularityChanged(granularity: number): void {
    this.methodCalled('onHighlightGranularityChanged', granularity);
    this.highlightGranularity = granularity;
  }

  onVoiceChange(voice: string, lang: string): void {
    this.methodCalled('onVoiceChange', voice, lang);
  }

  onLanguagePrefChange(lang: string, enabled: boolean): void {
    this.methodCalled('onLanguagePrefChange', lang, enabled);
    if (enabled) {
      this.languagesEnabledInPref.add(lang);
    } else {
      this.languagesEnabledInPref.delete(lang);
    }
  }

  sendGetVoicePackInfoRequest(lang: string): void {
    this.methodCalled('sendGetVoicePackInfoRequest', lang);
    this.requestInfoLangs.push(lang);
  }

  sendInstallVoicePackRequest(lang: string): void {
    this.methodCalled('sendInstallVoicePackRequest', lang);
    this.installedLangs.push(lang);
  }

  sendUninstallVoiceRequest(lang: string): void {
    this.methodCalled('sendUninstallVoiceRequest', lang);
    this.uninstalledLangs.push(lang);
  }

  onIsSpeechActiveChanged(isSpeechActive: boolean): void {
    this.methodCalled('onIsSpeechActiveChanged', isSpeechActive);
  }

  onIsAudioCurrentlyPlayingChanged(isAudioCurrentlyPlaying: boolean): void {
    this.methodCalled(
        'onIsAudioCurrentlyPlayingChanged', isAudioCurrentlyPlaying);
  }

  onSpeechEngineFirstStall(): void {
    this.methodCalled('onSpeechEngineFirstStall');
  }

  onSpeechEngineStalled(): void {
    this.methodCalled('onSpeechEngineStalled');
  }

  getKeyboardShortcutStopSource(): number {
    this.methodCalled('getKeyboardShortcutStopSource');
    return this.keyboardShortcutStopSource;
  }

  getEngineErrorStopSource(): number {
    this.methodCalled('getEngineErrorStopSource');
    return this.engineErrorStopSource;
  }

  getEngineInterruptStopSource(): number {
    this.methodCalled('getEngineInterruptStopSource');
    return this.engineInterruptStopSource;
  }

  getContentFinishedStopSource(): number {
    this.methodCalled('getContentFinishedStopSource');
    return this.contentFinishedStopSource;
  }

  getPauseButtonStopSource(): number {
    this.methodCalled('getPauseButtonStopSource');
    return this.pauseButtonStopSource;
  }

  getUnexpectedUpdateContentStopSource(): number {
    this.methodCalled('getUnexpectedUpdateContentStopSource');
    return this.unexpectedUpdateContentStopSource;
  }
}
