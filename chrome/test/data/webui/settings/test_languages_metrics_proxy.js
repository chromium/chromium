// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {LanguagesMetricsProxy} from 'chrome://settings/lazy_load.js';

import {TestBrowserProxy} from '../test_browser_proxy.m.js';

/**
 * A test version of LanguagesMetricsProxy.
 *
 * @implements {LanguagesMetricsProxy}
 *
 */
export class TestLanguagesMetricsProxy extends TestBrowserProxy {
  constructor() {
    super([
      'recordInteraction',
      'recordAddLanguages',
      'recordToggleSpellCheck',
      'recordToggleTranslate',
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
  recordToggleSpellCheck(value) {
    this.methodCalled('recordToggleSpellCheck', value);
  }

  /** @override */
  recordToggleTranslate(value) {
    this.methodCalled('recordToggleTranslate', value);
  }

  /** @override */
  recordTranslateCheckboxChanged(value) {
    this.methodCalled('recordTranslateCheckboxChanged', value);
  }
}
