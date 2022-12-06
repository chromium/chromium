// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {LanguagesBrowserProxy} from 'chrome://os-settings/chromeos/lazy_load.js';
import {FakeInputMethodPrivate} from './fake_input_method_private.js';
import {FakeLanguageSettingsPrivate} from './fake_language_settings_private.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

/** @implements {LanguagesBrowserProxy} */
export class TestLanguagesBrowserProxy extends TestBrowserProxy {
  constructor() {
    const methodNames = [];
    methodNames.push('getProspectiveUiLanguage', 'setProspectiveUiLanguage');

    super(methodNames);

    /** @private {!LanguageSettingsPrivate} */
    this.languageSettingsPrivate_ =
        new FakeLanguageSettingsPrivate();

    /** @private {!InputMethodPrivate} */
    this.inputMethodPrivate_ = /** @type{!InputMethodPrivate} */ (
        new FakeInputMethodPrivate());
  }

  /** @override */
  getLanguageSettingsPrivate() {
    return this.languageSettingsPrivate_;
  }

  /** @param {!LanguageSettingsPrivate} languageSettingsPrivate */
  setLanguageSettingsPrivate(languageSettingsPrivate) {
    this.languageSettingsPrivate_ = languageSettingsPrivate;
  }

  /** @override */
  getProspectiveUiLanguage() {
    this.methodCalled('getProspectiveUiLanguage');
    return Promise.resolve('en-US');
  }

  /** @override */
  setProspectiveUiLanguage(language) {
    this.methodCalled('setProspectiveUiLanguage', language);
  }


  getInputMethodPrivate() {
    return this.inputMethodPrivate_;
  }
}
