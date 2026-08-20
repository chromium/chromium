// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {AudioBrowserProxy} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {TestBrowserProxy} from 'chrome-untrusted://webui-test/test_browser_proxy.js';

export class TestAudioBrowserProxy extends TestBrowserProxy implements
    AudioBrowserProxy {
  speechRate: number = 1.0;
  autoHighlighting: number = 0;
  wordHighlighting: number = 1;
  phraseHighlighting: number = 2;
  sentenceHighlighting: number = 3;
  noHighlighting: number = 4;
  isPhraseHighlightingEnabledFlag: boolean = false;

  constructor() {
    super([
      'getSpeechRate',
      'getAutoHighlighting',
      'getWordHighlighting',
      'getPhraseHighlighting',
      'getSentenceHighlighting',
      'getNoHighlighting',
      'isPhraseHighlightingEnabled',
      'onSpeechRateChange',
      'onHighlightGranularityChanged',
    ]);
  }

  getSpeechRate(): number {
    this.methodCalled('getSpeechRate');
    return this.speechRate;
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

  onSpeechRateChange(rate: number): void {
    this.methodCalled('onSpeechRateChange', rate);
  }

  onHighlightGranularityChanged(granularity: number): void {
    this.methodCalled('onHighlightGranularityChanged', granularity);
  }
}
