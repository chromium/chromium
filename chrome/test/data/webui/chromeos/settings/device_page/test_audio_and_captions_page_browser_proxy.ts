// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {AudioAndCaptionsPageBrowserProxy} from 'chrome://os-settings/lazy_load.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestAudioAndCaptionsPageBrowserProxy extends TestBrowserProxy
    implements AudioAndCaptionsPageBrowserProxy {
  constructor() {
    super([
      'setStartupSoundEnabled',
      'audioAndCaptionsPageReady',
      'getStartupSoundEnabled',
      'previewFlashNotification',
    ]);
  }

  setStartupSoundEnabled(enabled: boolean): void {
    this.methodCalled('setStartupSoundEnabled', enabled);
    webUIListenerCallback('startup-sound-setting-retrieved', enabled);
  }

  audioAndCaptionsPageReady(): void {
    this.methodCalled('audioAndCaptionsPageReady');
  }

  getStartupSoundEnabled(): void {
    this.methodCalled('getStartupSoundEnabled');
  }

  previewFlashNotification(): void {
    this.methodCalled('previewFlashNotification');
  }
}
