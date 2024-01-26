// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {LanguagesBrowserProxy} from 'chrome://os-settings/lazy_load.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

import {FakeInputMethodPrivate} from '../fake_input_method_private.js';
import {FakeLanguageSettingsPrivate} from '../fake_language_settings_private.js';

type LanguageSettingsPrivate = typeof chrome.languageSettingsPrivate;
type InputMethodPrivate = typeof chrome.inputMethodPrivate;

export class TestLanguagesBrowserProxy extends TestBrowserProxy implements
    LanguagesBrowserProxy {
  private languageSettingsPrivate_: LanguageSettingsPrivate;
  private inputMethodPrivate_: InputMethodPrivate;
  constructor() {
    super([
      'getProspectiveUiLanguage',
      'setProspectiveUiLanguage',
      'getInputMethodPrivate',
      'getLanguageSettingsPrivate',
    ]);

    this.languageSettingsPrivate_ = new FakeLanguageSettingsPrivate();

    this.inputMethodPrivate_ =
        new FakeInputMethodPrivate() as unknown as InputMethodPrivate;
  }

  getLanguageSettingsPrivate(): LanguageSettingsPrivate {
    this.methodCalled('getLanguageSettingsPrivate');
    return this.languageSettingsPrivate_;
  }

  setLanguageSettingsPrivate(languageSettingsPrivate: LanguageSettingsPrivate):
      void {
    this.languageSettingsPrivate_ = languageSettingsPrivate;
  }

  getProspectiveUiLanguage(): Promise<string> {
    this.methodCalled('getProspectiveUiLanguage');
    return Promise.resolve('en-US');
  }

  setProspectiveUiLanguage(language: string): void {
    this.methodCalled('setProspectiveUiLanguage', language);
  }

  getInputMethodPrivate(): InputMethodPrivate {
    this.methodCalled('getInputMethodPrivate');
    return this.inputMethodPrivate_;
  }
}
