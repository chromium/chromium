// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {MetricsBrowserProxy, ReadAloudSettingsChange, ReadAnythingSettingsChange, ReadAnythingVoiceType} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {TestBrowserProxy} from 'chrome-untrusted://webui-test/test_browser_proxy.js';

// Test version of the BrowserProxy used in connecting Reading Mode to the color
// pipeline. The color pipeline is called in the connectedCallback when creating
// the app and creates mojo pipelines which we don't need to test here.
export class TestMetricsBrowserProxy extends TestBrowserProxy implements
    MetricsBrowserProxy {
  constructor() {
    super([
      'recordNewPage',
      'recordNewPageWithSpeech',
      'recordHighlightOn',
      'recordHighlightOff',
      'recordVoiceType',
      'recordLanguage',
      'recordTextSettingsChange',
      'recordSpeechSettingsChange',
      'recordVoiceSpeed',
    ]);
  }

  recordNewPage() {
    this.methodCalled('recordNewPage');
  }

  recordNewPageWithSpeech() {
    this.methodCalled('recordNewPageWithSpeech');
  }

  recordHighlightOn() {
    this.methodCalled('recordHighlightOn');
  }

  recordHighlightOff() {
    this.methodCalled('recordHighlightOff');
  }

  recordVoiceType(voiceType: ReadAnythingVoiceType) {
    this.methodCalled('recordVoiceType', voiceType);
  }

  recordLanguage(lang: string) {
    this.methodCalled('recordLanguage', lang);
  }

  recordTextSettingsChange(settingsChange: ReadAnythingSettingsChange) {
    this.methodCalled('recordTextSettingsChange', settingsChange);
  }

  recordSpeechSettingsChange(settingsChange: ReadAloudSettingsChange) {
    this.methodCalled('recordSpeechSettingsChange', settingsChange);
  }

  recordVoiceSpeed(index: number) {
    this.methodCalled('recordVoiceSpeed', index);
  }
}
