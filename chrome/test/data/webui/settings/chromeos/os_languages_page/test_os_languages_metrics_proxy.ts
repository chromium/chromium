// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {InputsShortcutReminderState, LanguagesMetricsProxy, LanguagesPageInteraction} from 'chrome://os-settings/lazy_load.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestLanguagesMetricsProxy extends TestBrowserProxy implements
    LanguagesMetricsProxy {
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

  recordInteraction(interaction: LanguagesPageInteraction): void {
    this.methodCalled('recordInteraction', interaction);
  }

  recordAddLanguages(): void {
    this.methodCalled('recordAddLanguages');
  }

  recordManageInputMethods(): void {
    this.methodCalled('recordManageInputMethods');
  }

  recordToggleShowInputOptionsOnShelf(value: boolean): void {
    this.methodCalled('recordToggleShowInputOptionsOnShelf', value);
  }

  recordToggleSpellCheck(value: boolean): void {
    this.methodCalled('recordToggleSpellCheck', value);
  }

  recordToggleTranslate(value: boolean): void {
    this.methodCalled('recordToggleTranslate', value);
  }

  recordAddInputMethod(): void {
    this.methodCalled('recordAddInputMethod');
  }

  recordTranslateCheckboxChanged(value: boolean): void {
    this.methodCalled('recordTranslateCheckboxChanged', value);
  }

  recordShortcutReminderDismissed(value: InputsShortcutReminderState): void {
    this.methodCalled('recordShortcutReminderDismissed', value);
  }
}
