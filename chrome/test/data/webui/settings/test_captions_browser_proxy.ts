// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CaptionsBrowserProxy, LiveCaptionLanguageList} from 'chrome://settings/lazy_load.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestCaptionsBrowserProxy extends TestBrowserProxy implements
    CaptionsBrowserProxy {
  private availableLanguagePacks_: LiveCaptionLanguageList;
  private installedLanguagePacks_: LiveCaptionLanguageList;

  constructor() {
    super([
      'openSystemCaptionsDialog',
      'liveCaptionSectionReady',
      'getInstalledLanguagePacks',
      'getAvailableLanguagePacks',
      'getTranslatableLanguages',
      'removeLanguagePack',
      'installLanguagePacks',
    ]);

    this.availableLanguagePacks_ = [
      {
        displayName: 'English',
        nativeDisplayName: 'English',
        code: 'en-US',
        downloadProgress: '0%',
      },
      {
        displayName: 'French',
        nativeDisplayName: 'French',
        code: 'fr-FR',
        downloadProgress: '0%',
      },
    ];

    this.installedLanguagePacks_ = [{
      displayName: 'English',
      nativeDisplayName: 'English',
      code: 'en-US',
      downloadProgress: '0%',
    }];
  }

  openSystemCaptionsDialog() {
    this.methodCalled('openSystemCaptionsDialog');
    return Promise.resolve();
  }

  liveCaptionSectionReady() {
    this.methodCalled('liveCaptionSectionReady');
    return Promise.resolve();
  }

  getInstalledLanguagePacks() {
    this.methodCalled('getInstalledLanguagePacks');
    return Promise.resolve(this.installedLanguagePacks_);
  }

  getAvailableLanguagePacks() {
    this.methodCalled('getAvailableLanguagePacks');
    return Promise.resolve(this.availableLanguagePacks_);
  }

  removeLanguagePack(languageCode: string) {
    this.methodCalled('removeLanguagePack', languageCode);
  }

  installLanguagePacks(languageCodes: string[]) {
    this.methodCalled('installLanguagePacks', languageCodes);
  }
}
