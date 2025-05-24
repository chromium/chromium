// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {SpeechBrowserProxy} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {TestBrowserProxy} from 'chrome-untrusted://webui-test/test_browser_proxy.js';

export class TestSpeechBrowserProxy extends TestBrowserProxy implements
    SpeechBrowserProxy {
  private voices_: SpeechSynthesisVoice[] = [];

  constructor() {
    super([
      'cancel',
      'getVoices',
      'pause',
      'resume',
      'setOnVoicesChanged',
      'speak',
    ]);
  }

  cancel() {
    this.methodCalled('cancel');
  }

  setVoices(voices: SpeechSynthesisVoice[]) {
    this.voices_ = voices;
  }

  getVoices(): SpeechSynthesisVoice[] {
    this.methodCalled('getVoices');
    return this.voices_;
  }

  pause() {
    this.methodCalled('pause');
  }

  resume() {
    this.methodCalled('resume');
  }

  setOnVoicesChanged(onvoiceschanged: (event: Event) => any) {
    this.methodCalled('setOnVoicesChanged', onvoiceschanged);
  }

  speak(utterance: SpeechSynthesisUtterance) {
    this.methodCalled('speak', utterance);
  }
}
