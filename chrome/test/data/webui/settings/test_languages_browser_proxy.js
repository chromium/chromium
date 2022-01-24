// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {isWindows} from 'chrome://resources/js/cr.m.js';

import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

import {FakeLanguageSettingsPrivate} from './fake_language_settings_private.js';
// clang-format on

/** @implements {LanguagesBrowserProxy} */
export class TestLanguagesBrowserProxy extends TestBrowserProxy {
  constructor() {
    const methodNames = [];
    if (isWindows) {
      methodNames.push('getProspectiveUILanguage', 'setProspectiveUILanguage');
    }

    super(methodNames);

    /** @private {!LanguageSettingsPrivate} */
    this.languageSettingsPrivate_ = new FakeLanguageSettingsPrivate();
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

if (isWindows) {
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
