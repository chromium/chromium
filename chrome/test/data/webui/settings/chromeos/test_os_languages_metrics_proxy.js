// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {LanguagesMetricsProxy} from 'chrome://os-settings/chromeos/lazy_load.js';
// #import {TestBrowserProxy} from '../../test_browser_proxy.m.js';
// clang-format on

cr.define('settings', function() {
  /**
   * A test version of LanguagesMetricsProxy.
   * @implements {settings.LanguagesMetricsProxy}
   */
  /* #export */ class TestLanguagesMetricsProxy extends TestBrowserProxy {
    constructor() {
      super([
        'recordInteraction',
        'recordAddLanguages',
        'recordManageInputMethods',
        'recordToggleShowInputOptionsOnShelf',
        'recordToggleSpellCheck',
        'recordToggleTranslate',
        'recordAddInputMethod',
        'recordTranslateCheckboxChanged',
      ]);
    }

    /** @override */
    recordInteraction(interaction) {
      this.methodCalled('recordInteraction', interaction);
    }

    /** @override */
    recordAddLanguages() {
      this.methodCalled('recordAddLanguages');
    }

    /** @override */
    recordManageInputMethods() {
      this.methodCalled('recordManageInputMethods');
    }

    /** @override */
    recordToggleShowInputOptionsOnShelf(value) {
      this.methodCalled('recordToggleShowInputOptionsOnShelf', value);
    }

    /** @override */
    recordToggleSpellCheck(value) {
      this.methodCalled('recordToggleSpellCheck', value);
    }

    /** @override */
    recordToggleTranslate(value) {
      this.methodCalled('recordToggleTranslate', value);
    }

    /** @override */
    recordAddInputMethod(value) {
      this.methodCalled('recordAddInputMethod', value);
    }

    /** @override */
    recordTranslateCheckboxChanged(value) {
      this.methodCalled('recordTranslateCheckboxChanged', value);
    }
  }
  // #cr_define_end
  return {
    TestLanguagesMetricsProxy: TestLanguagesMetricsProxy,
  };
});
