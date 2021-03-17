// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {isChromeOS, isWindows} from 'chrome://resources/js/cr.m.js';
import {LanguagesBrowserProxy} from 'chrome://settings/lazy_load.js';

import {TestBrowserProxy} from '../test_browser_proxy.m.js';

import {FakeInputMethodPrivate} from './fake_input_method_private.js';
import {FakeLanguageSettingsPrivate} from './fake_language_settings_private.js';
// clang-format on

/** @implements {LanguagesBrowserProxy} */
export class TestLanguagesBrowserProxy extends TestBrowserProxy {
  constructor() {
    const methodNames = [];
    if (isChromeOS || isWindows) {
      methodNames.push('getProspectiveUILanguage', 'setProspectiveUILanguage');
    }

    super(methodNames);

    /** @private {!LanguageSettingsPrivate} */
    this.languageSettingsPrivate_ = new FakeLanguageSettingsPrivate();

    /** @private {!InputMethodPrivate} */
    this.inputMethodPrivate_ =
        /** @type{!InputMethodPrivate} */ (new FakeInputMethodPrivate());
  }

  /** @override */
  getLanguageSettingsPrivate() {
    return this.languageSettingsPrivate_;
  }

  /** @param {!LanguageSettingsPrivate} languageSettingsPrivate */
  setLanguageSettingsPrivate(languageSettingsPrivate) {
    this.languageSettingsPrivate_ = languageSettingsPrivate;
  }
}

if (isChromeOS || isWindows) {
  /** @override */
  TestLanguagesBrowserProxy.prototype.getProspectiveUILanguage = function() {
    this.methodCalled('getProspectiveUILanguage');
    return Promise.resolve('en-US');
  };

  /** @override */
  TestLanguagesBrowserProxy.prototype.setProspectiveUILanguage = function(
      language) {
    this.methodCalled('setProspectiveUILanguage', language);
  };
}

if (isChromeOS) {
  /** @override */
  TestLanguagesBrowserProxy.prototype.getInputMethodPrivate = function() {
    return this.inputMethodPrivate_;
  };
}
