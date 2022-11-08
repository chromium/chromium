// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {LanguagesMetricsProxy} from 'chrome://os-settings/chromeos/lazy_load.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

/**
 * A test version of LanguagesMetricsProxy.
 * @implements {LanguagesMetricsProxy}
 */
export class TestLanguagesMetricsProxy extends TestBrowserProxy {
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
      'recordShortcutReminderDismissed',
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

  /** @override */
  recordShortcutReminderDismissed(value) {
    this.methodCalled('recordShortcutReminderDismissed', value);
  }
}
