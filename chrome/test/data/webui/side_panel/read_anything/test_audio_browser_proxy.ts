// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {AudioBrowserProxy} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {TestBrowserProxy} from 'chrome-untrusted://webui-test/test_browser_proxy.js';

export class TestAudioBrowserProxy extends TestBrowserProxy implements
    AudioBrowserProxy {
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
  storedVoice: string = 'abc';
  languagesEnabledInPref: Set<string> = new Set<string>();
  installedLangs: string[] = [];
  uninstalledLangs: string[] = [];
  requestInfoLangs: string[] = [];
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
}
