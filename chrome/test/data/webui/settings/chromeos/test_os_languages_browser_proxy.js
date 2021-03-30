// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {LanguagesBrowserProxy} from 'chrome://os-settings/chromeos/lazy_load.js';
// #import {FakeInputMethodPrivate} from '../fake_input_method_private.js';
// #import {FakeLanguageSettingsPrivate} from '../fake_language_settings_private.js';
// #import {TestBrowserProxy} from '../../test_browser_proxy.m.js';
// clang-format on

cr.define('settings', function() {
  /** @implements {settings.LanguagesBrowserProxy} */
  /* #export */ class TestLanguagesBrowserProxy extends TestBrowserProxy {
    constructor() {
      const methodNames = [];
      methodNames.push('getProspectiveUILanguage', 'setProspectiveUILanguage');

      super(methodNames);

      /** @private {!LanguageSettingsPrivate} */
      this.languageSettingsPrivate_ =
          new settings.FakeLanguageSettingsPrivate();

      /** @private {!InputMethodPrivate} */
      this.inputMethodPrivate_ = /** @type{!InputMethodPrivate} */ (
          new settings.FakeInputMethodPrivate());
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
    getProspectiveUILanguage() {
      this.methodCalled('getProspectiveUILanguage');
      return Promise.resolve('en-US');
    }

    /** @override */
    setProspectiveUILanguage(language) {
      this.methodCalled('setProspectiveUILanguage', language);
    }


    getInputMethodPrivate() {
      return this.inputMethodPrivate_;
    }
  }

  // #cr_define_end
  return {
    TestLanguagesBrowserProxy: TestLanguagesBrowserProxy,
  };
});
