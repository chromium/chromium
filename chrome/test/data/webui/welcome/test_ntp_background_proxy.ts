// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import type {NtpBackgroundData, NtpBackgroundProxy} from 'chrome://welcome/ntp_background/ntp_background_proxy.js';

export class TestNtpBackgroundProxy extends TestBrowserProxy implements
    NtpBackgroundProxy {
  private backgroundsList_: NtpBackgroundData[] = [];
  private preloadImageSuccess_: boolean = true;

  constructor() {
    super([
      'clearBackground',
      'getBackgrounds',
      'preloadImage',
      'recordBackgroundImageFailedToLoad',
      'setBackground',
    ]);
  }

  clearBackground() {
    this.methodCalled('clearBackground');
  }

  getBackgrounds() {
    this.methodCalled('getBackgrounds');
    return Promise.resolve(this.backgroundsList_);
  }

  preloadImage(url: string) {
    this.methodCalled('preloadImage', url);
    return this.preloadImageSuccess_ ? Promise.resolve() : Promise.reject();
  }

  recordBackgroundImageFailedToLoad() {
    this.methodCalled('recordBackgroundImageFailedToLoad');
  }

  recordBackgroundImageNeverLoaded() {}

  setBackground(id: number) {
    this.methodCalled('setBackground', id);
  }

  setPreloadImageSuccess(success: boolean) {
    this.preloadImageSuccess_ = success;
  }

  setBackgroundsList(backgroundsList: NtpBackgroundData[]) {
    this.backgroundsList_ = backgroundsList;
  }
}
