// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {AudioBrowserProxy} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {TestBrowserProxy} from 'chrome-untrusted://webui-test/test_browser_proxy.js';

export class TestAudioBrowserProxy extends TestBrowserProxy implements
    AudioBrowserProxy {
  speechRate: number = 1.0;

  constructor() {
    super([
      'getSpeechRate',
      'onSpeechRateChange',
    ]);
  }

  getSpeechRate(): number {
    this.methodCalled('getSpeechRate');
    return this.speechRate;
  }

  onSpeechRateChange(rate: number): void {
    this.methodCalled('onSpeechRateChange', rate);
  }
}
