// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

/**
 * A test version of LanguagesMetricsProxy.
 */
// This class implements LanguagesMetricsProxy from
// c/b/r/ash/settings/os_languages_page/languages_metrics_proxy.ts.
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
