// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {LanguagesBrowserProxy} from 'chrome://settings/lazy_load.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

import {FakeLanguageSettingsPrivate} from './fake_language_settings_private.js';

// clang-format on

export class TestLanguagesBrowserProxy extends TestBrowserProxy implements
    LanguagesBrowserProxy {
  private languageSettingsPrivate_: typeof chrome.languageSettingsPrivate =
      new FakeLanguageSettingsPrivate() as unknown as
      typeof chrome.languageSettingsPrivate;

  // <if expr="is_win">
  constructor() {
    super([
      'getProspectiveUILanguage',
      'setProspectiveUILanguage',
    ]);
  }
  // </if>

  getLanguageSettingsPrivate() {
    return this.languageSettingsPrivate_;
  }

  setLanguageSettingsPrivate(languageSettingsPrivate:
                                 typeof chrome.languageSettingsPrivate) {
    this.languageSettingsPrivate_ = languageSettingsPrivate;
  }

  // <if expr="is_win">
  getProspectiveUILanguage() {
    this.methodCalled('getProspectiveUILanguage');
    return Promise.resolve('en-US');
  }

  setProspectiveUILanguage(language: string) {
    this.methodCalled('setProspectiveUILanguage', language);
  }
  // </if>
}
